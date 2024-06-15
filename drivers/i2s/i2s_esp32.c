/*
 * Copyright (c) 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT espressif_esp32_i2s

#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/dma/dma_esp32.h>
#include <soc.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>

#include "esp_check.h"
#include "i2s_esp32.h"
LOG_MODULE_REGISTER(i2s_esp32);

#if DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 0
#error "I2S series coder is not defined in DTS"
#endif

#include "esp_clk_tree.h"

#define IS2_ESP32_CLK_SRC  I2S_CLK_SRC_DEFAULT

#define I2S_ESP32_RX_BLOCK_COUNT 2 /*<--*/
#define I2S_ESP32_TX_BLOCK_COUNT 2 /*<--*/

#define I2S_ESP32_MODULO_INC(val, max) { val = (++val < max) ? val : 0; }

static int i2s_esp32_enable_clock(const struct device *dev)
{
	const struct i2s_esp32_cfg *dev_cfg = dev->config;
	const struct device *clk_dev = dev_cfg->clock_dev;
	int ret;

	if (!device_is_ready(clk_dev)) {
		LOG_ERR("clock control device not ready");
		return -ENODEV;
	}

	ret = clock_control_off(clk_dev, dev_cfg->clock_subsys);
	ret = clock_control_on(clk_dev, dev_cfg->clock_subsys);
	if (ret != 0) {
		LOG_ERR("Could not enable I2S clock");
		return -EIO;
	}

	return 0;
}

uint32_t i2s_esp32_get_source_clk_freq(i2s_clock_src_t clk_src, uint32_t mclk_freq_hz)
{
	uint32_t clk_freq = 0;

	esp_clk_tree_src_get_freq_hz(clk_src, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &clk_freq);
	return clk_freq;
}

#if SOC_I2S_SUPPORTS_ADC
#if CONFIG_I2S_ESP32_MODE_ADC_BUILT_IN || CONFIG_I2S_ESP32_MODE_DAC_BUILT_IN
static esp_err_t i2s_calculate_adc_dac_clock(const struct i2s_config *i2s_cfg,
					     i2s_hal_clock_info_t *i2s_hal_clock_info)
{
	return ESP_OK;
}
#endif /* CONFIG_I2S_ESP32_MODE_ADC_BUILT_IN || CONFIG_I2S_ESP32_MODE_DAC_BUILT_IN */
#endif /* SOC_I2S_SUPPORTS_ADC */

#if SOC_I2S_SUPPORTS_PDM_TX && CONFIG_I2S_ESP32_MODE_PDM_TX
static esp_err_t i2s_calculate_pdm_tx_clock(const struct i2s_config *i2s_cfg,
					    i2s_hal_clock_info_t *i2s_hal_clock_info)
{
	return ESP_OK;
}
#endif /* SOC_I2S_SUPPORTS_PDM_TX && CONFIG_I2S_ESP32_MODE_PDM_TX */

#if SOC_I2S_SUPPORTS_PDM_RX && CONFIG_I2S_ESP32_MODE_PDM_RX
static esp_err_t i2s_calculate_pdm_rx_clock(const struct i2s_config *i2s_cfg,
					    i2s_hal_clock_info_t *i2s_hal_clock_info)
{
	return ESP_OK;
}
#endif /* SOC_I2S_SUPPORTS_PDM_RX && CONFIG_I2S_ESP32_MODE_PDM_RX */

static esp_err_t i2s_esp32_calculate_common_clock(const struct i2s_config *i2s_cfg,
						  i2s_hal_clock_info_t *i2s_hal_clock_info)
{
	if (i2s_cfg == NULL) {
		LOG_ERR("Input i2s_cfg is NULL");
		return ESP_ERR_INVALID_ARG;
	}

	if (i2s_hal_clock_info == NULL) {
		LOG_ERR("Input hal_clock_info is NULL");
		return ESP_ERR_INVALID_ARG;
	}

	/* For words greater than 16-bit the channel length is considered 32-bit */
	const uint8_t channel_length = i2s_cfg->word_size > 16U ? 32U : 16U;

	uint16_t mclk_multiple = 256;

	if (i2s_cfg->options & I2S_OPT_FRAME_CLK_SLAVE ||
	    i2s_cfg->options & I2S_OPT_BIT_CLK_SLAVE) {
		i2s_hal_clock_info->bclk_div = 8;
		i2s_hal_clock_info->bclk = i2s_cfg->frame_clk_freq * i2s_cfg->channels
					   * channel_length;
		i2s_hal_clock_info->mclk = i2s_cfg->frame_clk_freq
					   * i2s_hal_clock_info->bclk_div;
	} else {
		i2s_hal_clock_info->bclk = i2s_cfg->frame_clk_freq * i2s_cfg->channels
					   * channel_length;
		i2s_hal_clock_info->mclk = i2s_cfg->frame_clk_freq * mclk_multiple;
		i2s_hal_clock_info->bclk_div = i2s_hal_clock_info->mclk
					       / i2s_hal_clock_info->bclk;
	}

	i2s_hal_clock_info->sclk =
	       i2s_esp32_get_source_clk_freq(IS2_ESP32_CLK_SRC, i2s_hal_clock_info->mclk);

	i2s_hal_clock_info->mclk_div = i2s_hal_clock_info->sclk /
				       i2s_hal_clock_info->mclk;
	if (i2s_hal_clock_info->mclk_div == 0) {
		LOG_ERR("Sample rate is too large for the current clock source");
		return ESP_ERR_INVALID_ARG;
	}

	return ESP_OK;
}

static esp_err_t i2s_esp32_calculate_clock(const struct i2s_config *i2s_cfg,
					   i2s_hal_clock_info_t *i2s_hal_clock_info)
{
	esp_err_t ret;

#if SOC_I2S_SUPPORTS_ADC && CONFIG_I2S_ESP32_MODE_ADC_BUILT_IN
	ret = i2s_calculate_adc_dac_clock(i2s_cfg, i2s_hal_clock_info);
	if (ret != ESP_OK) {
		LOG_ERR("ADC clock calculate failed");
	}
	return ret;
#endif /* SOC_I2S_SUPPORTS_ADC && CONFIG_I2S_ESP32_MODE_ADC_BUILT_IN */

#if SOC_I2S_SUPPORTS_DAC && CONFIG_I2S_ESP32_MODE_DAC_BUILT_IN
	ret = i2s_calculate_adc_dac_clock(i2s_cfg, i2s_hal_clock_info);
	if (ret != ESP_OK) {
		LOG_ERR("DAC clock calculate failed");
	}
	return ret;
#endif /* SOC_I2S_SUPPORTS_DAC && CONFIG_I2S_ESP32_MODE_DAC_BUILT_IN */

#if SOC_I2S_SUPPORTS_PDM_TX && CONFIG_I2S_ESP32_MODE_PDM_TX
	ret = i2s_calculate_pdm_tx_clock(i2s_cfg, i2s_hal_clock_info);
	if (ret != ESP_OK) {
		LOG_ERR("PDM TX clock calculate failed");
	}
	return ret;
#endif /* SOC_I2S_SUPPORTS_PDM_TX && CONFIG_I2S_ESP32_MODE_PDM_TX */

#if SOC_I2S_SUPPORTS_PDM_RX && CONFIG_I2S_ESP32_MODE_PDM_RX
	ret = i2s_calculate_pdm_rx_clock(i2s_cfg, i2s_hal_clock_info);
	if (ret != ESP_OK) {
		LOG_ERR("PDM RX clock calculate failed");
	}
	return ret;
#endif /* SOC_I2S_SUPPORTS_PDM_RX && CONFIG_I2S_ESP32_MODE_PDM_RX */

	ret = i2s_esp32_calculate_common_clock(i2s_cfg, i2s_hal_clock_info);
	if (ret != ESP_OK) {
		LOG_ERR("Common clock calculate failed");
	}
	return ret;
}

static int i2s_esp32_queue_get(struct ring_buffer *rb, void **mem_block, size_t *size)
{
	unsigned int key;

	key = irq_lock();

	if (rb->tail == rb->head) {
		irq_unlock(key);
		return -ENOMEM;
	}

	*mem_block = rb->array[rb->tail].buffer;
	*size = rb->array[rb->tail].size;
	I2S_ESP32_MODULO_INC(rb->tail, rb->len);

	irq_unlock(key);

	return 0;
}

static int i2s_esp32_queue_put(struct ring_buffer *rb, void *mem_block, size_t size)
{
	uint16_t head_next;
	unsigned int key;

	key = irq_lock();

	head_next = rb->head;
	I2S_ESP32_MODULO_INC(head_next, rb->len);

	if (head_next == rb->tail) {
		irq_unlock(key);
		return -ENOMEM;
	}

	rb->array[rb->head].buffer = mem_block;
	rb->array[rb->head].size = size;
	rb->head = head_next;

	irq_unlock(key);

	return 0;
}

static void i2s_esp32_rx_queue_drop(struct i2s_esp32_stream *stream)
{
	size_t size;
	void *mem_block;

	while (i2s_esp32_queue_get(&stream->queue, &mem_block, &size) == 0) {
		k_mem_slab_free(stream->i2s_cfg.mem_slab, &mem_block);
	}

	k_sem_reset(&stream->queue.sem);
}

static void i2s_esp32_tx_queue_drop(struct i2s_esp32_stream *stream)
{
	size_t size;
	void *mem_block;
	unsigned int n = 0U;

	while (i2s_esp32_queue_get(&stream->queue, &mem_block, &size) == 0) {
		k_mem_slab_free(stream->i2s_cfg.mem_slab, &mem_block);
		n++;
	}

	for (; n > 0; n--) {
		k_sem_give(&stream->queue.sem);
	}
}

static void i2s_esp32_dma_rx_callback(const struct device *dma_dev, void *arg, uint32_t channel,
				      int status);
static void i2s_esp32_dma_tx_callback(const struct device *dma_dev, void *arg, uint32_t channel,
				      int status);

static int i2s_esp32_restart_dma(const struct device *dev, enum i2s_dir i2s_dir)
{
	const struct i2s_esp32_cfg *dev_cfg = dev->config;
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = NULL;
	void *src, *dst;

	if (i2s_dir == I2S_DIR_RX) {
		stream = &dev_data->rx;
		src = NULL;
		dst = stream->mem_block;
	} else if (i2s_dir == I2S_DIR_TX) {
		stream = &dev_data->tx;
		src = stream->mem_block;
		dst = NULL;
	} else {
		LOG_ERR("Invalid dma direction");
		return -EINVAL;
	}

	int err;

	err = dma_reload(stream->dma_dev, stream->dma_channel, (uint32_t)src, (uint32_t)dst,
				stream->mem_block_len);
	if (err < 0) {
		return -EIO;
	}

	err = dma_start(stream->dma_dev, stream->dma_channel);
	if (err < 0) {
		return -EIO;
	}

	return 0;
}

static int i2s_esp32_start_dma(const struct device *dev, enum i2s_dir i2s_dir)
{
	const struct i2s_esp32_cfg *dev_cfg = dev->config;
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = NULL;

	if (i2s_dir == I2S_DIR_RX) {
		stream = &dev_data->rx;
	} else if (i2s_dir == I2S_DIR_TX) {
		stream = &dev_data->tx;
	} else {
		LOG_ERR("Invalid dma direction");
		return -EINVAL;
	}

	int err = 0;
	struct dma_status dma_status = {0};

	err = dma_get_status(stream->dma_dev, stream->dma_channel, &dma_status);
	if (err) {
		LOG_ERR("Unable to get dma channel[%u] status: %d",
			(unsigned int)stream->dma_channel, err);
		return -EINVAL;
	}

	if (dma_status.busy) {
		LOG_ERR("dma channel[%u] is busy!", (unsigned int)stream->dma_channel);
		return -EBUSY;
	}

	struct dma_config dma_cfg = {0};
	struct dma_block_config dma_blk = {0};
	unsigned int key = irq_lock();

	dma_blk.block_size = stream->mem_block_len;
	if (i2s_dir == I2S_DIR_RX) {
		dma_blk.dest_address = (uint32_t)stream->mem_block;
		dma_cfg.channel_direction = PERIPHERAL_TO_MEMORY;
		dma_cfg.dma_callback = i2s_esp32_dma_rx_callback;
	} else {
		dma_blk.source_address = (uint32_t)stream->mem_block;
		dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
		dma_cfg.dma_callback = i2s_esp32_dma_tx_callback;
	}
	dma_cfg.user_data = (void *)dev;
	dma_cfg.dma_slot = dev_cfg->i2s_num == 0 ? ESP_GDMA_TRIG_PERIPH_I2S0 :
						   ESP_GDMA_TRIG_PERIPH_I2S1;
	dma_cfg.block_count = 1;
	dma_cfg.head_block = &dma_blk;

	int ret = 0;

	err = dma_config(stream->dma_dev, stream->dma_channel, &dma_cfg);
	if (err) {
		LOG_ERR("Error configuring dma: %d", err);
		ret = -EINVAL;
		goto unlock;
	}

	err = dma_start(stream->dma_dev, stream->dma_channel);
	if (err) {
		LOG_ERR("Error starting dma: %d", err);
		ret = -EIO;
		goto unlock;
	}

unlock:
	irq_unlock(key);
	return ret;
}

static int i2s_esp32_rx_stream_start(const struct device *dev)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = &dev_data->rx;
	i2s_hal_context_t *hal_cxt = &dev_data->hal_cxt;
	int err;

	err = k_mem_slab_alloc(stream->i2s_cfg.mem_slab, &stream->mem_block, K_NO_WAIT);
	if (err < 0) {
		return -ENOMEM;
	}
	stream->mem_block_len = stream->i2s_cfg.block_size;

	i2s_hal_rx_stop(hal_cxt);/*<--*/
	i2s_hal_rx_reset(hal_cxt);
	i2s_hal_rx_reset_fifo(hal_cxt);

	err = i2s_esp32_start_dma(dev, I2S_DIR_RX);
	if (err < 0) {
		LOG_ERR("Failed to start RX DMA transfer: %d", err);
		return -EIO;
	}

	i2s_hal_rx_start(hal_cxt);

	return 0;
}

static int i2s_esp32_tx_stream_start(const struct device *dev)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = &dev_data->tx;
	i2s_hal_context_t *hal_cxt = &dev_data->hal_cxt;
	int err;

	err = i2s_esp32_queue_get(&stream->queue, &stream->mem_block, &stream->mem_block_len);
	if (err < 0) {
		return -ENOMEM;
	}
	k_sem_give(&stream->queue.sem);

	i2s_hal_tx_stop(hal_cxt);/*<--*/
	i2s_hal_tx_reset(hal_cxt);
	i2s_hal_tx_reset_fifo(hal_cxt);

	err = i2s_esp32_start_dma(dev, I2S_DIR_TX);
	if (err < 0) {
		LOG_ERR("Failed to start TX DMA transfer: %d", err);
		return -EIO;
	}

	i2s_hal_tx_start(hal_cxt);

	return 0;
}

static void i2s_esp32_rx_stream_stop(const struct device *dev)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = &dev_data->rx;
	i2s_hal_context_t *hal_cxt = &dev_data->hal_cxt;

	dma_stop(stream->dma_dev, stream->dma_channel);
	if (stream->mem_block != NULL) {
		k_mem_slab_free(stream->i2s_cfg.mem_slab, &stream->mem_block);
		stream->mem_block = NULL;
		stream->mem_block_len = 0;
	}

	i2s_hal_rx_stop(hal_cxt);
}

static void i2s_esp32_tx_stream_stop(const struct device *dev)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = &dev_data->tx;
	i2s_hal_context_t *hal_cxt = &dev_data->hal_cxt;

	dma_stop(stream->dma_dev, stream->dma_channel);
	if (stream->mem_block != NULL) {
		k_mem_slab_free(stream->i2s_cfg.mem_slab, &stream->mem_block);
		stream->mem_block = NULL;
	}

	i2s_hal_tx_stop(hal_cxt);
}

static void i2s_esp32_dma_rx_callback(const struct device *dma_dev, void *arg, uint32_t channel,
				      int status)
{
	const struct device *dev =  (const struct device *)arg;
	const struct i2s_esp32_cfg *dev_cfg = dev->config;
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = &dev_data->rx;

	if (status < 0) {
		LOG_DBG("RX status bad: %d", status);
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}

	__ASSERT_NO_MSG(stream->mem_block != NULL);

	if (stream->state == I2S_STATE_ERROR) {
		goto rx_disable;
	}

	int err;

	err = i2s_esp32_queue_put(&stream->queue, stream->mem_block, stream->mem_block_len);
	if (err < 0) {
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}
	k_sem_give(&stream->queue.sem);

	if (stream->state == I2S_STATE_STOPPING) {
		stream->state = I2S_STATE_READY;
		goto rx_disable;
	}

	err = k_mem_slab_alloc(stream->i2s_cfg.mem_slab, &stream->mem_block, K_NO_WAIT);
	if (err < 0) {
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}
	stream->mem_block_len = stream->i2s_cfg.block_size;

	err = i2s_esp32_restart_dma(dev, I2S_DIR_RX);
	if (err < 0) {
		goto rx_disable;
	}

	return;

rx_disable:
	i2s_esp32_rx_stream_stop(dev);

	if (stream->state == I2S_STATE_ERROR) {
		LOG_ERR("RX error detected");
	}
}
uint32_t i2s_tx_callback_counter = 0;
static void i2s_esp32_dma_tx_callback(const struct device *dma_dev, void *arg, uint32_t channel,
				      int status)
{
	const struct device *dev = (const struct device *)arg;
	const struct i2s_esp32_cfg *dev_cfg = dev->config;
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream = &dev_data->tx;
	size_t mem_block_size;
LOG_ERR("i2s_tx_callback_counter = %u", (unsigned int)++i2s_tx_callback_counter);

	if (status < 0) {
		LOG_DBG("TX bad status: %d", status);
		stream->state = I2S_STATE_ERROR;
	}

	__ASSERT_NO_MSG(stream->mem_block != NULL);

	k_mem_slab_free(stream->i2s_cfg.mem_slab, &stream->mem_block);
	stream->mem_block = NULL;

	if (stream->state == I2S_STATE_ERROR) {
		goto tx_disable;
	}

	if (stream->last_block) {
		stream->state = I2S_STATE_READY;
		goto tx_disable;
	}

	int err;

	err = i2s_esp32_queue_get(&stream->queue, &stream->mem_block, &mem_block_size);
	if (err < 0) {
		if (stream->state == I2S_STATE_STOPPING) {
			stream->state = I2S_STATE_READY;
		} else {
			stream->state = I2S_STATE_ERROR;
		}
		goto tx_disable;
	}
	k_sem_give(&stream->queue.sem);

	err = i2s_esp32_restart_dma(dev, I2S_DIR_TX);
	if (err < 0) {
		goto tx_disable;
	}

	return;

tx_disable:
	i2s_esp32_tx_stream_stop(dev);

	if (stream->state == I2S_STATE_ERROR) {
		LOG_ERR("TX error detected");
	}
}

#include "soc/periph_defs.h"
#include "esp_private/periph_ctrl.h"

static int i2s_esp32_initialize(const struct device *dev)
{
	const struct i2s_esp32_cfg *dev_cfg = dev->config;
	struct i2s_esp32_data *const dev_data = dev->data;
	int err;

	err = i2s_esp32_enable_clock(dev);
	if (err < 0) {
		LOG_ERR("%s: clock enabling failed: %d", __func__, err);
		return -EIO;
	}

	err = pinctrl_apply_state(dev_cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		LOG_ERR("I2S pinctrl setup failed (%d)", err);
		return  -EIO;
	}

	err = k_sem_init(&dev_data->rx.queue.sem, 0, I2S_ESP32_RX_BLOCK_COUNT);
	if (err != 0) {
		return -EINVAL;
	}

	err = k_sem_init(&dev_data->tx.queue.sem, I2S_ESP32_TX_BLOCK_COUNT,
			 I2S_ESP32_TX_BLOCK_COUNT);
	if (err != 0) {
		return -EINVAL;
	}

	if (!device_is_ready(dev_data->tx.dma_dev)) {
		LOG_ERR("%s device not ready", dev_data->tx.dma_dev->name);
		return -ENODEV;
	}

	if (!device_is_ready(dev_data->rx.dma_dev)) {
		LOG_ERR("%s device not ready", dev_data->rx.dma_dev->name);
		return -ENODEV;
	}

	periph_module_enable(PERIPH_I2S0_MODULE);
	i2s_hal_init(&dev_data->hal_cxt, dev_cfg->i2s_num);
	i2s_ll_enable_clock(dev_data->hal_cxt.dev);

	LOG_INF("%s inited", dev->name);

	return 0;
}

static int i2s_esp32_configure(const struct device *dev, enum i2s_dir dir,
			       const struct i2s_config *i2s_cfg)
{
	const struct i2s_esp32_cfg *const dev_cfg = dev->config;
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream;

	switch (dir) {
	case I2S_DIR_RX:
		stream = &dev_data->rx;
		break;
	case I2S_DIR_TX:
		stream = &dev_data->tx;
		break;
	case I2S_DIR_BOTH:
		LOG_ERR("I2S_DIR_BOTH is not supported");
		return -ENOSYS;
	default:
		LOG_ERR("Either RX or TX direction must be selected");
		return -EINVAL;
	}

	if (stream->state != I2S_STATE_NOT_READY &&
	    stream->state != I2S_STATE_READY) {
		LOG_ERR("invalid state");
		return -EINVAL;
	}

	stream->master = true;
	if (i2s_cfg->options & I2S_OPT_FRAME_CLK_SLAVE ||
	    i2s_cfg->options & I2S_OPT_BIT_CLK_SLAVE) {
		stream->master = false;
	}

	if (i2s_cfg->frame_clk_freq == 0U) {
		stream->queue_drop(stream);
		memset(&stream->i2s_cfg, 0, sizeof(struct i2s_config));
		stream->state = I2S_STATE_NOT_READY;
		return 0;
	}

	memcpy(&stream->i2s_cfg, i2s_cfg, sizeof(struct i2s_config));

	switch (i2s_cfg->format & I2S_FMT_DATA_FORMAT_MASK) {
	case I2S_FMT_DATA_FORMAT_I2S:
		break;
	case I2S_FMT_DATA_FORMAT_PCM_SHORT:
		break;
	case I2S_FMT_DATA_FORMAT_PCM_LONG:
		break;
	default:
		LOG_ERR("I2S format not supported");
		return -ENOSYS;
	}

	int ret;
	uint32_t bit_clk_freq;
	i2s_hal_clock_info_t i2s_hal_clock_info;

	ret = i2s_esp32_calculate_clock(i2s_cfg, &i2s_hal_clock_info);
	if (ret != ESP_OK) {
		return -EINVAL;
	}

	bool is_slave;

	if (stream->master) {
		is_slave = false;
	} else {
		is_slave = true;
	}

	if (dir == I2S_DIR_RX) {
		i2s_hal_set_rx_clock(&dev_data->hal_cxt, &i2s_hal_clock_info, IS2_ESP32_CLK_SRC);
	} else if (dir == I2S_DIR_TX) {
		i2s_hal_set_tx_clock(&dev_data->hal_cxt, &i2s_hal_clock_info, IS2_ESP32_CLK_SRC);
	}

	stream->state = I2S_STATE_READY;

	return 0;
}

static const struct i2s_config *i2s_esp32_config_get(const struct device *dev,
						    enum i2s_dir dir)
{
	struct i2s_esp32_data *dev_data = dev->data;
	struct i2s_esp32_stream *stream;

	if (dir == I2S_DIR_RX) {
		stream = &dev_data->rx;
	} else {
		stream = &dev_data->tx;
	}

	if (stream->state == I2S_STATE_NOT_READY) {
		return NULL;
	}

	return &stream->i2s_cfg;
}

static int i2s_esp32_trigger(const struct device *dev, enum i2s_dir dir, enum i2s_trigger_cmd cmd)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	struct i2s_esp32_stream *stream;
	unsigned int key;
	int ret;

	switch (dir) {
	case I2S_DIR_RX:
		stream = &dev_data->rx;
		break;
	case I2S_DIR_TX:
		stream = &dev_data->tx;
		break;
	case I2S_DIR_BOTH:
		return -ENOSYS;
	default:
		LOG_ERR("Either RX or TX direction must be selected");
		return -EINVAL;
	}

	switch (cmd) {
	case I2S_TRIGGER_START:
		if (stream->state != I2S_STATE_READY) {
			LOG_ERR("[START] Invalid stream state: %d", stream->state);
			return -EIO;
		}

		ret = stream->stream_start(dev);
		if (ret < 0) {
			LOG_ERR("[START] Stream start failed: %d", ret);
			return ret;
		}
		stream->last_block = false;
		stream->state = I2S_STATE_RUNNING;
		break;

	case I2S_TRIGGER_STOP:
		key = irq_lock();
		if (stream->state != I2S_STATE_RUNNING) {
			irq_unlock(key);
			LOG_ERR("[STOP] Invalid stream state: %d", stream->state);
			return -EIO;
		}
		stream->stream_stop(dev);
		stream->queue_drop(stream);
		stream->last_block = true;
		stream->state = I2S_STATE_READY;
		irq_unlock(key);
		break;

	case I2S_TRIGGER_DRAIN:
		key = irq_lock();
		if (stream->state != I2S_STATE_RUNNING) {
			irq_unlock(key);
			LOG_ERR("[DRAIN] Invalid stream state: %d", stream->state);
			return -EIO;
		}
		stream->stream_stop(dev);
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		irq_unlock(key);
		break;

	case I2S_TRIGGER_DROP:
		if (stream->state == I2S_STATE_NOT_READY) {
			LOG_ERR("[DROP]  Invalid stream state: %d", stream->state);
			return -EIO;
		}
		stream->stream_stop(dev);
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		break;

	case I2S_TRIGGER_PREPARE:
		if (stream->state != I2S_STATE_ERROR) {
			LOG_ERR("[PREPARE]  Invalid stream state: %d", stream->state);
			return -EIO;
		}
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		break;

	default:
		LOG_ERR("Unsupported trigger command");
		return -EINVAL;
	}

	return 0;
}

static int i2s_esp32_buffer_read(const struct device *dev, void **mem_block, size_t *size)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	int err;

	if (dev_data->rx.state == I2S_STATE_NOT_READY ||
	    dev_data->rx.state == I2S_STATE_ERROR ) {
		LOG_ERR("RX stream invalid state: %d", (int)dev_data->rx.state);
		return -EIO;
	}

	err = k_sem_take(&dev_data->rx.queue.sem, SYS_TIMEOUT_MS(dev_data->rx.i2s_cfg.timeout));
	if (err < 0) {
		return err;
	}
	(void)i2s_esp32_queue_get(&dev_data->rx.queue, mem_block, size);

	return 0;
}
uint32_t i2s_write_counter = 0;
static int i2s_esp32_buffer_write(const struct device *dev, void *mem_block, size_t size)
{
	struct i2s_esp32_data *const dev_data = dev->data;
	int err;
LOG_ERR("i2s_write_counter = %u", (unsigned int)++i2s_write_counter);
	if (dev_data->tx.state != I2S_STATE_RUNNING &&
	    dev_data->tx.state != I2S_STATE_READY) {
		LOG_ERR("TX stream invalid state: %d", (int)dev_data->tx.state);
		return -EIO;
	}

	err = k_sem_take(&dev_data->tx.queue.sem, SYS_TIMEOUT_MS(dev_data->tx.i2s_cfg.timeout));
	if (err < 0) {
		return err;
	}
	(void)i2s_esp32_queue_put(&dev_data->tx.queue, mem_block, size);

	return 0;
}

static const struct i2s_driver_api i2s_esp32_driver_api = {
	.configure = i2s_esp32_configure,
	.config_get = i2s_esp32_config_get,
	.trigger = i2s_esp32_trigger,
	.read = i2s_esp32_buffer_read,
	.write = i2s_esp32_buffer_write,
};

#define I2S(idx) DT_NODELABEL(i2s##idx)

#define I2S_ESP32_DMA_CHANNEL_INIT(index, dir)                                                     \
	.dir = {                                                                                   \
		.state = I2S_STATE_NOT_READY,	                                                   \
		.master = false,                                                                   \
		.dma_dev = DEVICE_DT_GET(DT_INST_DMAS_CTLR_BY_NAME(index, dir)),                   \
		.dma_channel = DT_INST_DMAS_CELL_BY_NAME(index, dir, channel),                     \
		.mem_block = NULL,                                                                 \
		.mem_block_len = 0,                                                                \
		.stream_start = i2s_esp32_##dir##_stream_start,                                    \
		.stream_stop = i2s_esp32_##dir##_stream_stop,                                      \
		.queue_drop = i2s_esp32_##dir##_queue_drop,                                        \
		.queue.array = i2s_esp32_##dir##_##index##_queue_item_array,                       \
		.queue.len = ARRAY_SIZE(i2s_esp32_##dir##_##index##_queue_item_array),             \
		.last_block = true,	                                                           \
	}

#define I2S_ESP32_INIT(index)                                                                      \
	PINCTRL_DT_INST_DEFINE(index);                                                             \
                                                                                                   \
	static const struct i2s_esp32_cfg i2s_esp32_config_##index = {                             \
		.i2s_num = index,                                                                  \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(index),                                     \
		.clock_dev = DEVICE_DT_GET(DT_CLOCKS_CTLR(I2S(index))),                            \
		.clock_subsys = (clock_control_subsys_t)DT_CLOCKS_CELL(I2S(index), offset),        \
	};                                                                                         \
                                                                                                   \
	static struct queue_item i2s_esp32_rx_##index##_queue_item_array[I2S_ESP32_RX_BLOCK_COUNT];\
	static struct queue_item i2s_esp32_tx_##index##_queue_item_array[I2S_ESP32_TX_BLOCK_COUNT];\
                                                                                                   \
	static struct i2s_esp32_data i2s_esp32_data_##index = {                                    \
		.hal_cxt =                                                                         \
			{                                                                          \
				.dev = (i2s_dev_t *)DT_REG_ADDR(I2S(index)),                       \
			},                                                                         \
		UTIL_AND(DT_INST_DMAS_HAS_NAME(index, rx),                                         \
			 I2S_ESP32_DMA_CHANNEL_INIT(index, rx)),                                   \
		UTIL_AND(DT_INST_DMAS_HAS_NAME(index, tx),                                         \
			 I2S_ESP32_DMA_CHANNEL_INIT(index, tx)),                                   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(index, &i2s_esp32_initialize, NULL, &i2s_esp32_data_##index,         \
			      &i2s_esp32_config_##index, POST_KERNEL, CONFIG_I2S_INIT_PRIORITY,    \
			      &i2s_esp32_driver_api);

DT_INST_FOREACH_STATUS_OKAY(I2S_ESP32_INIT)
