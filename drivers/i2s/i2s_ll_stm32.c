/*
 * Copyright (c) 2018 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <drivers/dma.h>
#include <drivers/i2s.h>
#include <dt-bindings/dma/stm32_dma.h>
#include <soc.h>
#include <drivers/clock_control/stm32_clock_control.h>
#include <drivers/clock_control.h>

#include "i2s_ll_stm32.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(i2s_ll_stm32);

/* FIXME change to
 * #if __DCACHE_PRESENT == 1
 * when cache support is added
 */
#if 0
#define DCACHE_INVALIDATE(addr, size) \
	SCB_InvalidateDCache_by_Addr((u32_t *)addr, size)
#define DCACHE_CLEAN(addr, size) \
	SCB_CleanDCache_by_Addr((u32_t *)addr, size)
#else
#define DCACHE_INVALIDATE(addr, size) {; }
#define DCACHE_CLEAN(addr, size) {; }
#endif

#define MODULO_INC(val, max) { val = (++val < max) ? val : 0; }

static unsigned int div_round_closest(u32_t dividend, u32_t divisor)
{
	return (dividend + (divisor / 2U)) / divisor;
}

/*
 * Get data from the queue
 */
static int queue_get(struct ring_buf *rb, void **mem_block, size_t *size)
{
	unsigned int key;

	key = irq_lock();

	if (rb->tail == rb->head) {
		/* Ring buffer is empty */
		irq_unlock(key);
		return -ENOMEM;
	}

	*mem_block = rb->buf[rb->tail].mem_block;
	*size = rb->buf[rb->tail].size;
	MODULO_INC(rb->tail, rb->len);

	irq_unlock(key);

	return 0;
}

/*
 * Put data in the queue
 */
static int queue_put(struct ring_buf *rb, void *mem_block, size_t size)
{
	u16_t head_next;
	unsigned int key;

	key = irq_lock();

	head_next = rb->head;
	MODULO_INC(head_next, rb->len);

	if (head_next == rb->tail) {
		/* Ring buffer is full */
		irq_unlock(key);
		return -ENOMEM;
	}

	rb->buf[rb->head].mem_block = mem_block;
	rb->buf[rb->head].size = size;
	rb->head = head_next;

	irq_unlock(key);

	return 0;
}

static int i2s_stm32_enable_clock(struct device *dev)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct device *clk;
	int ret;

	clk = device_get_binding(STM32_CLOCK_CONTROL_NAME);
	__ASSERT_NO_MSG(clk);

	ret = clock_control_on(clk, (clock_control_subsys_t *) &cfg->pclken);
	if (ret != 0) {
		LOG_ERR("Could not enable I2S clock");
		return -EIO;
	}

	return 0;
}

#ifdef CONFIG_I2S_STM32_USE_PLLI2S_ENABLE
#define PLLI2S_MAX_MS_TIME	1 /* PLLI2S lock time is 300us max */
static u16_t plli2s_ms_count;

#define z_pllr(v) LL_RCC_PLLI2SR_DIV_ ## v
#define pllr(v) z_pllr(v)
#endif

static int i2s_stm32_set_clock(struct device *dev, u32_t bit_clk_freq)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	u32_t pll_src = LL_RCC_PLL_GetMainSource();
	int freq_in;
	u8_t i2s_div, i2s_odd;

	freq_in = (pll_src == LL_RCC_PLLSOURCE_HSI) ?
		   HSI_VALUE : CONFIG_CLOCK_STM32_HSE_CLOCK;

#ifdef CONFIG_I2S_STM32_USE_PLLI2S_ENABLE
	/* Set PLLI2S */
	LL_RCC_PLLI2S_Disable();
	LL_RCC_PLLI2S_ConfigDomain_I2S(pll_src,
				       CONFIG_I2S_STM32_PLLI2S_PLLM,
				       CONFIG_I2S_STM32_PLLI2S_PLLN,
				       pllr(CONFIG_I2S_STM32_PLLI2S_PLLR));
	LL_RCC_PLLI2S_Enable();

	/* wait until PLLI2S gets locked */
	while (!LL_RCC_PLLI2S_IsReady()) {
		if (plli2s_ms_count++ > PLLI2S_MAX_MS_TIME) {
			return -EIO;
		}

		/* wait 1 ms */
		k_sleep(K_MSEC(1));
	}
	LOG_DBG("PLLI2S is locked");

	/* Adjust freq_in according to PLLM, PLLN, PLLR */
	float freq_tmp;

	freq_tmp = freq_in / CONFIG_I2S_STM32_PLLI2S_PLLM;
	freq_tmp *= CONFIG_I2S_STM32_PLLI2S_PLLN;
	freq_tmp /= CONFIG_I2S_STM32_PLLI2S_PLLR;
	freq_in = (int) freq_tmp;
#endif /* CONFIG_I2S_STM32_USE_PLLI2S_ENABLE */

	/* Select clock source */
	LL_RCC_SetI2SClockSource(cfg->i2s_clk_sel);

	/*
	 * The ratio between input clock (I2SxClk) and output
	 * clock on the pad (I2S_CK) is obtained using the
	 * following formula:
	 *   (i2s_div * 2) + i2s_odd
	 */
	i2s_div = div_round_closest(freq_in, bit_clk_freq);
	i2s_odd = (i2s_div & 0x1) ? 1 : 0;
	i2s_div >>= 1;

	LOG_DBG("i2s_div: %d - i2s_odd: %d", i2s_div, i2s_odd);

	LL_I2S_SetPrescalerLinear(cfg->i2s, i2s_div);
	LL_I2S_SetPrescalerParity(cfg->i2s, i2s_odd);

	return 0;
}

static int i2s_stm32_configure(struct device *dev, enum i2s_dir dir,
			       struct i2s_config *i2s_cfg)
{
	const struct i2s_stm32_cfg *const cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct stream *stream;
	u32_t bit_clk_freq;
	int ret;

	if (dir == I2S_DIR_RX) {
		stream = &dev_data->rx;
	} else if (dir == I2S_DIR_TX) {
		stream = &dev_data->tx;
	} else {
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
		memset(&stream->cfg, 0, sizeof(struct i2s_config));
		stream->state = I2S_STATE_NOT_READY;
		return 0;
	}

	memcpy(&stream->cfg, i2s_cfg, sizeof(struct i2s_config));

	/* set I2S bitclock */
	bit_clk_freq = i2s_cfg->frame_clk_freq *
		       i2s_cfg->word_size * i2s_cfg->channels;

	ret = i2s_stm32_set_clock(dev, bit_clk_freq);
	if (ret < 0) {
		return ret;
	}

	/* set I2S Master Clock */
	if (stream->master) {
		LL_I2S_EnableMasterClock(cfg->i2s);
	} else {
		LL_I2S_DisableMasterClock(cfg->i2s);
	}

	/* set I2S Data Format */
	if (i2s_cfg->word_size == 16U) {
		LL_I2S_SetDataFormat(cfg->i2s, LL_I2S_DATAFORMAT_16B);
	} else if (i2s_cfg->word_size == 24U) {
		LL_I2S_SetDataFormat(cfg->i2s, LL_I2S_DATAFORMAT_24B);
	} else if (i2s_cfg->word_size == 32U) {
		LL_I2S_SetDataFormat(cfg->i2s, LL_I2S_DATAFORMAT_32B);
	} else {
		LOG_ERR("invalid word size");
		return -EINVAL;
	}

	/* set I2S Standard */
	switch (i2s_cfg->format & I2S_FMT_DATA_FORMAT_MASK) {
	case I2S_FMT_DATA_FORMAT_I2S:
		LL_I2S_SetStandard(cfg->i2s, LL_I2S_STANDARD_PHILIPS);
		break;

	case I2S_FMT_DATA_FORMAT_PCM_SHORT:
		LL_I2S_SetStandard(cfg->i2s, LL_I2S_STANDARD_PCM_SHORT);
		break;

	case I2S_FMT_DATA_FORMAT_PCM_LONG:
		LL_I2S_SetStandard(cfg->i2s, LL_I2S_STANDARD_PCM_LONG);
		break;

	case I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED:
		LL_I2S_SetStandard(cfg->i2s, LL_I2S_STANDARD_MSB);
		break;

	case I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED:
		LL_I2S_SetStandard(cfg->i2s, LL_I2S_STANDARD_LSB);
		break;

	default:
		LOG_ERR("Unsupported I2S data format");
		return -EINVAL;
	}

	/* set I2S clock polarity */
	if ((i2s_cfg->format & I2S_FMT_CLK_FORMAT_MASK) == I2S_FMT_BIT_CLK_INV)
		LL_I2S_SetClockPolarity(cfg->i2s, LL_I2S_POLARITY_HIGH);
	else
		LL_I2S_SetClockPolarity(cfg->i2s, LL_I2S_POLARITY_LOW);

	stream->state = I2S_STATE_READY;
	return 0;
}

static int i2s_stm32_trigger(struct device *dev, enum i2s_dir dir,
			     enum i2s_trigger_cmd cmd)
{
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct stream *stream;
	unsigned int key;
	int ret;

	if (dir == I2S_DIR_RX) {
		stream = &dev_data->rx;
	} else if (dir == I2S_DIR_TX) {
		stream = &dev_data->tx;
	} else {
		LOG_ERR("Either RX or TX direction must be selected");
		return -EINVAL;
	}

	switch (cmd) {
	case I2S_TRIGGER_START:
		if (stream->state != I2S_STATE_READY) {
			LOG_ERR("START trigger: invalid state %d",
				    stream->state);
			return -EIO;
		}

		__ASSERT_NO_MSG(stream->mem_block == NULL);

		ret = stream->stream_start(stream, dev);
		if (ret < 0) {
			LOG_ERR("START trigger failed %d", ret);
			return ret;
		}

		stream->state = I2S_STATE_RUNNING;
		stream->last_block = false;
		break;

	case I2S_TRIGGER_STOP:
		key = irq_lock();
		if (stream->state != I2S_STATE_RUNNING) {
			irq_unlock(key);
			LOG_ERR("STOP trigger: invalid state");
			return -EIO;
		}
		irq_unlock(key);
		stream->stream_disable(stream, dev);
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		stream->last_block = true;
		break;

	case I2S_TRIGGER_DRAIN:
		key = irq_lock();
		if (stream->state != I2S_STATE_RUNNING) {
			irq_unlock(key);
			LOG_ERR("DRAIN trigger: invalid state");
			return -EIO;
		}
		stream->stream_disable(stream, dev);
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		irq_unlock(key);
		break;

	case I2S_TRIGGER_DROP:
		if (stream->state == I2S_STATE_NOT_READY) {
			LOG_ERR("DROP trigger: invalid state");
			return -EIO;
		}
		stream->stream_disable(stream, dev);
		stream->queue_drop(stream);
		stream->state = I2S_STATE_READY;
		break;

	case I2S_TRIGGER_PREPARE:
		if (stream->state != I2S_STATE_ERROR) {
			LOG_ERR("PREPARE trigger: invalid state");
			return -EIO;
		}
		stream->state = I2S_STATE_READY;
		stream->queue_drop(stream);
		break;

	default:
		LOG_ERR("Unsupported trigger command");
		return -EINVAL;
	}

	return 0;
}

static int i2s_stm32_read(struct device *dev, void **mem_block, size_t *size)
{
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	int ret;

	if (dev_data->rx.state == I2S_STATE_NOT_READY) {
		LOG_DBG("invalid state");
		return -EIO;
	}

	if (dev_data->rx.state != I2S_STATE_ERROR) {
		ret = k_sem_take(&dev_data->rx.sem,
				 K_MSEC(dev_data->rx.cfg.timeout));
		if (ret < 0) {
			return ret;
		}
	}

	/* Get data from the beginning of RX queue */
	ret = queue_get(&dev_data->rx.mem_block_queue, mem_block, size);
	if (ret < 0) {
		return -EIO;
	}

	return 0;
}

static int i2s_stm32_write(struct device *dev, void *mem_block, size_t size)
{
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	int ret;

	if (dev_data->tx.state != I2S_STATE_RUNNING &&
	    dev_data->tx.state != I2S_STATE_READY) {
		LOG_DBG("invalid state");
		return -EIO;
	}

	ret = k_sem_take(&dev_data->tx.sem, K_MSEC(dev_data->tx.cfg.timeout));
	if (ret < 0) {
		return ret;
	}

	/* Add data to the end of the TX queue */
	queue_put(&dev_data->tx.mem_block_queue, mem_block, size);

	return 0;
}

static const struct i2s_driver_api i2s_stm32_driver_api = {
	.configure = i2s_stm32_configure,
	.read = i2s_stm32_read,
	.write = i2s_stm32_write,
	.trigger = i2s_stm32_trigger,
};

#define STM32_DMA_NUM_CHANNELS		8
static struct device *active_dma_rx_channel[STM32_DMA_NUM_CHANNELS];
static struct device *active_dma_tx_channel[STM32_DMA_NUM_CHANNELS];

static int reload_dma(struct device *dev_dma, u32_t channel,
		      struct dma_config *dcfg, void *src, void *dst,
		      u32_t blk_size)
{
	int ret;

	ret = dma_reload(dev_dma, channel, (u32_t)src, (u32_t)dst, blk_size);
	if (ret < 0) {
		return ret;
	}

	ret = dma_start(dev_dma, channel);

	return ret;
}

static int start_dma(struct device *dev_dma, u32_t channel,
		     struct dma_config *dcfg, void *src,
		     bool src_addr_increment, void *dst,
		     bool dst_addr_increment, u8_t fifo_threshold,
		     u32_t blk_size)
{
	struct dma_block_config blk_cfg;
	int ret;

	memset(&blk_cfg, 0, sizeof(blk_cfg));
	blk_cfg.block_size = blk_size;
	blk_cfg.source_address = (u32_t)src;
	blk_cfg.dest_address = (u32_t)dst;
	if (src_addr_increment) {
		blk_cfg.source_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	} else {
		blk_cfg.source_addr_adj = DMA_ADDR_ADJ_NO_CHANGE;
	}
	if (dst_addr_increment) {
		blk_cfg.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	} else {
		blk_cfg.dest_addr_adj = DMA_ADDR_ADJ_NO_CHANGE;
	}
	blk_cfg.fifo_mode_control = fifo_threshold;

	dcfg->head_block = &blk_cfg;

	ret = dma_config(dev_dma, channel, dcfg);
	if (ret < 0) {
		return ret;
	}

	ret = dma_start(dev_dma, channel);

	return ret;
}

static struct device *get_dev_from_rx_dma_channel(u32_t dma_channel);
static struct device *get_dev_from_tx_dma_channel(u32_t dma_channel);
static void rx_stream_disable(struct stream *stream, struct device *dev);
static void tx_stream_disable(struct stream *stream, struct device *dev);

/* This function is executed in the interrupt context */
static void dma_rx_callback(void *arg, u32_t channel, int status)
{
	struct device *dev = get_dev_from_rx_dma_channel(channel);
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct stream *stream = &dev_data->rx;
	void *mblk_tmp;
	int ret;

	if (status != 0) {
		ret = -EIO;
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}

	__ASSERT_NO_MSG(stream->mem_block != NULL);

	/* Stop reception if there was an error */
	if (stream->state == I2S_STATE_ERROR) {
		goto rx_disable;
	}

	mblk_tmp = stream->mem_block;

	/* Prepare to receive the next data block */
	ret = k_mem_slab_alloc(stream->cfg.mem_slab, &stream->mem_block,
			       K_NO_WAIT);
	if (ret < 0) {
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}

	ret = reload_dma(dev_data->dev_dma_rx, stream->dma_channel,
			&stream->dma_cfg,
			(void *)LL_SPI_DMA_GetRegAddr(cfg->i2s),
			stream->mem_block,
			stream->cfg.block_size);
	if (ret < 0) {
		LOG_DBG("Failed to start RX DMA transfer: %d", ret);
		goto rx_disable;
	}

	/* Assure cache coherency after DMA write operation */
	DCACHE_INVALIDATE(mblk_tmp, stream->cfg.block_size);

	/* All block data received */
	ret = queue_put(&stream->mem_block_queue, mblk_tmp,
			stream->cfg.block_size);
	if (ret < 0) {
		stream->state = I2S_STATE_ERROR;
		goto rx_disable;
	}
	k_sem_give(&stream->sem);

	/* Stop reception if we were requested */
	if (stream->state == I2S_STATE_STOPPING) {
		stream->state = I2S_STATE_READY;
		goto rx_disable;
	}

	return;

rx_disable:
	rx_stream_disable(stream, dev);
}

static void dma_tx_callback(void *arg, u32_t channel, int status)
{
	struct device *dev = get_dev_from_tx_dma_channel(channel);
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct stream *stream = &dev_data->tx;
	size_t mem_block_size;
	int ret;

	if (status != 0) {
		ret = -EIO;
		stream->state = I2S_STATE_ERROR;
		goto tx_disable;
	}

	__ASSERT_NO_MSG(stream->mem_block != NULL);

	/* All block data sent */
	k_mem_slab_free(stream->cfg.mem_slab, &stream->mem_block);
	stream->mem_block = NULL;

	/* Stop transmission if there was an error */
	if (stream->state == I2S_STATE_ERROR) {
		LOG_ERR("TX error detected");
		goto tx_disable;
	}

	/* Stop transmission if we were requested */
	if (stream->last_block) {
		stream->state = I2S_STATE_READY;
		goto tx_disable;
	}

	/* Prepare to send the next data block */
	ret = queue_get(&stream->mem_block_queue, &stream->mem_block,
			&mem_block_size);
	if (ret < 0) {
		if (stream->state == I2S_STATE_STOPPING) {
			stream->state = I2S_STATE_READY;
		} else {
			stream->state = I2S_STATE_ERROR;
		}
		goto tx_disable;
	}
	k_sem_give(&stream->sem);

	/* Assure cache coherency before DMA read operation */
	DCACHE_CLEAN(stream->mem_block, mem_block_size);

	ret = reload_dma(dev_data->dev_dma_tx, stream->dma_channel,
			&stream->dma_cfg,
			stream->mem_block,
			(void *)LL_SPI_DMA_GetRegAddr(cfg->i2s),
			stream->cfg.block_size);
	if (ret < 0) {
		LOG_DBG("Failed to start TX DMA transfer: %d", ret);
		goto tx_disable;
	}

	return;

tx_disable:
	tx_stream_disable(stream, dev);
}

static u32_t i2s_stm32_irq_count;
static u32_t i2s_stm32_irq_ovr_count;

static void i2s_stm32_isr(void *arg)
{
	struct device *const dev = (struct device *) arg;
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct stream *stream = &dev_data->rx;

	LOG_ERR("%s: err=%d", __func__, (int)LL_I2S_ReadReg(cfg->i2s, SR));
	stream->state = I2S_STATE_ERROR;

	/* OVR error must be explicitly cleared */
	if (LL_I2S_IsActiveFlag_OVR(cfg->i2s)) {
		i2s_stm32_irq_ovr_count++;
		LL_I2S_ClearFlag_OVR(cfg->i2s);
	}

	i2s_stm32_irq_count++;
}

static int i2s_stm32_initialize(struct device *dev)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	int ret, i;

	/* Enable I2S clock propagation */
	ret = i2s_stm32_enable_clock(dev);
	if (ret < 0) {
		LOG_ERR("%s: clock enabling failed: %d",  __func__, ret);
		return -EIO;
	}

	cfg->irq_config(dev);

	k_sem_init(&dev_data->rx.sem, 0, CONFIG_I2S_STM32_RX_BLOCK_COUNT);
	k_sem_init(&dev_data->tx.sem, CONFIG_I2S_STM32_TX_BLOCK_COUNT,
		   CONFIG_I2S_STM32_TX_BLOCK_COUNT);

	for (i = 0; i < STM32_DMA_NUM_CHANNELS; i++) {
		active_dma_rx_channel[i] = NULL;
		active_dma_tx_channel[i] = NULL;
	}

	/* Get the binding to the DMA device */
	dev_data->dev_dma_tx = device_get_binding(dev_data->tx.dma_name);
	if (!dev_data->dev_dma_tx) {
		LOG_ERR("%s device not found", dev_data->tx.dma_name);
		return -ENODEV;
	}
	dev_data->dev_dma_rx = device_get_binding(dev_data->rx.dma_name);
	if (!dev_data->dev_dma_rx) {
		LOG_ERR("%s device not found", dev_data->rx.dma_name);
		return -ENODEV;
	}

	LOG_INF("%s inited", dev->config->name);

	return 0;
}

static int rx_stream_start(struct stream *stream, struct device *dev)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	int ret;

	ret = k_mem_slab_alloc(stream->cfg.mem_slab, &stream->mem_block,
			       K_NO_WAIT);
	if (ret < 0) {
		return ret;
	}

	if (stream->master) {
		LL_I2S_SetTransferMode(cfg->i2s, LL_I2S_MODE_MASTER_RX);
	} else {
		LL_I2S_SetTransferMode(cfg->i2s, LL_I2S_MODE_SLAVE_RX);
	}

	/* remember active RX DMA channel (used in callback) */
	active_dma_rx_channel[stream->dma_channel] = dev;

	ret = start_dma(dev_data->dev_dma_rx, stream->dma_channel,
			&stream->dma_cfg,
			(void *)LL_SPI_DMA_GetRegAddr(cfg->i2s),
			stream->src_addr_increment, stream->mem_block,
			stream->dst_addr_increment, stream->fifo_threshold,
			stream->cfg.block_size);
	if (ret < 0) {
		LOG_ERR("Failed to start RX DMA transfer: %d", ret);
		return ret;
	}

	LL_I2S_EnableDMAReq_RX(cfg->i2s);

	LL_I2S_EnableIT_ERR(cfg->i2s);
	LL_I2S_Enable(cfg->i2s);

	return 0;
}

static int tx_stream_start(struct stream *stream, struct device *dev)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	size_t mem_block_size;
	int ret;

	ret = queue_get(&stream->mem_block_queue, &stream->mem_block,
			&mem_block_size);
	if (ret < 0) {
		return ret;
	}
	k_sem_give(&stream->sem);

	/* Assure cache coherency before DMA read operation */
	DCACHE_CLEAN(stream->mem_block, mem_block_size);

	if (stream->master) {
		LL_I2S_SetTransferMode(cfg->i2s, LL_I2S_MODE_MASTER_TX);
	} else {
		LL_I2S_SetTransferMode(cfg->i2s, LL_I2S_MODE_SLAVE_TX);
	}

	/* remember active TX DMA channel (used in callback) */
	active_dma_tx_channel[stream->dma_channel] = dev;

	ret = start_dma(dev_data->dev_dma_tx, stream->dma_channel,
			&stream->dma_cfg,
			stream->mem_block, stream->src_addr_increment,
			(void *)LL_SPI_DMA_GetRegAddr(cfg->i2s),
			stream->dst_addr_increment, stream->fifo_threshold,
			stream->cfg.block_size);
	if (ret < 0) {
		LOG_ERR("Failed to start TX DMA transfer: %d", ret);
		return ret;
	}

	LL_I2S_EnableDMAReq_TX(cfg->i2s);

	LL_I2S_EnableIT_ERR(cfg->i2s);
	LL_I2S_Enable(cfg->i2s);

	return 0;
}

static void rx_stream_disable(struct stream *stream, struct device *dev)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct device *dev_dma = dev_data->dev_dma_rx;

	LL_I2S_DisableDMAReq_RX(cfg->i2s);
	LL_I2S_DisableIT_ERR(cfg->i2s);

	dma_stop(dev_dma, stream->dma_channel);
	if (stream->mem_block != NULL) {
		k_mem_slab_free(stream->cfg.mem_slab, &stream->mem_block);
		stream->mem_block = NULL;
	}

	LL_I2S_Disable(cfg->i2s);

	active_dma_rx_channel[stream->dma_channel] = NULL;
}

static void tx_stream_disable(struct stream *stream, struct device *dev)
{
	const struct i2s_stm32_cfg *cfg = DEV_CFG(dev);
	struct i2s_stm32_data *const dev_data = DEV_DATA(dev);
	struct device *dev_dma = dev_data->dev_dma_tx;

	LL_I2S_DisableDMAReq_TX(cfg->i2s);
	LL_I2S_DisableIT_ERR(cfg->i2s);

	dma_stop(dev_dma, stream->dma_channel);
	if (stream->mem_block != NULL) {
		k_mem_slab_free(stream->cfg.mem_slab, &stream->mem_block);
		stream->mem_block = NULL;
	}

	LL_I2S_Disable(cfg->i2s);

	active_dma_tx_channel[stream->dma_channel] = NULL;
}

static void rx_queue_drop(struct stream *stream)
{
	size_t size;
	void *mem_block;

	while (queue_get(&stream->mem_block_queue, &mem_block, &size) == 0) {
		k_mem_slab_free(stream->cfg.mem_slab, &mem_block);
	}

	k_sem_reset(&stream->sem);
}

static void tx_queue_drop(struct stream *stream)
{
	size_t size;
	void *mem_block;
	unsigned int n = 0U;

	while (queue_get(&stream->mem_block_queue, &mem_block, &size) == 0) {
		k_mem_slab_free(stream->cfg.mem_slab, &mem_block);
		n++;
	}

	for (; n > 0; n--) {
		k_sem_give(&stream->sem);
	}
}

static struct device *get_dev_from_rx_dma_channel(u32_t dma_channel)
{
	return active_dma_rx_channel[dma_channel];
}

static struct device *get_dev_from_tx_dma_channel(u32_t dma_channel)
{
	return active_dma_tx_channel[dma_channel];
}

/* src_dev and dest_dev should be 'MEM' or 'PERIPH'. */
#define I2S_DMA_CHANNEL_INIT(index, dir, dir_cap, src_dev, dest_dev)	\
.dir = {								\
	.dma_name = DT_I2S_##index##_DMA_CONTROLLER_##dir_cap,		\
	.dma_channel = DT_I2S_##index##_DMA_CHANNEL_##dir_cap,		\
	.dma_cfg = {							\
		.block_count = 2,					\
		.dma_slot = DT_I2S_##index##_DMA_SLOT_##dir_cap,	\
		.channel_direction = PERIPHERAL_TO_MEMORY,		\
		.source_data_size = 2,  /* 16bit default */		\
		.dest_data_size = 2,    /* 16bit default */		\
		.source_burst_length = 0, /* SINGLE transfer */		\
		.dest_burst_length = 1,					\
		.channel_priority = STM32_DMA_CONFIG_PRIORITY(		\
			DT_I2S_##index##_DMA_CHANNEL_CONFIG_##dir_cap),	\
		.dma_callback = dma_##dir##_callback,			\
	},								\
	.src_addr_increment = STM32_DMA_CONFIG_##src_dev##_ADDR_INC(	\
		DT_I2S_##index##_DMA_CHANNEL_CONFIG_##dir_cap),		\
	.dst_addr_increment = STM32_DMA_CONFIG_##dest_dev##_ADDR_INC(	\
		DT_I2S_##index##_DMA_CHANNEL_CONFIG_##dir_cap),		\
	.fifo_threshold = STM32_DMA_FEATURES_FIFO_THRESHOLD(		\
		DT_I2S_##index##_DMA_FEATURES_##dir_cap),		\
	.stream_start = dir##_stream_start,				\
	.stream_disable = dir##_stream_disable,				\
	.queue_drop = dir##_queue_drop,					\
	.mem_block_queue.buf = dir##_##index##_ring_buf,		\
	.mem_block_queue.len = ARRAY_SIZE(dir##_##index##_ring_buf),	\
}

#define I2S_INIT(index, clk_sel)					\
static struct device DEVICE_NAME_GET(i2s_stm32_##index);		\
									\
static void i2s_stm32_irq_config_func_##index(struct device *dev);	\
									\
static const struct i2s_stm32_cfg i2s_stm32_config_##index = {		\
	.i2s = (SPI_TypeDef *) DT_I2S_##index##_BASE_ADDRESS,		\
	.pclken = {							\
		.enr = DT_I2S_##index##_CLOCK_BITS,			\
		.bus = DT_I2S_##index##_CLOCK_BUS,			\
	},								\
	.i2s_clk_sel = CLK_SEL_##clk_sel,				\
	.irq_config = i2s_stm32_irq_config_func_##index,		\
};									\
									\
struct queue_item rx_##index##_ring_buf[CONFIG_I2S_STM32_RX_BLOCK_COUNT + 1];\
struct queue_item tx_##index##_ring_buf[CONFIG_I2S_STM32_TX_BLOCK_COUNT + 1];\
									\
static struct i2s_stm32_data i2s_stm32_data_##index = {			\
	I2S_DMA_CHANNEL_INIT(index, rx, RX, PERIPH, MEM),		\
	I2S_DMA_CHANNEL_INIT(index, tx, TX, MEM, PERIPH),		\
};									\
DEVICE_AND_API_INIT(i2s_stm32_##index, DT_I2S_##index##_NAME,		\
		    &i2s_stm32_initialize, &i2s_stm32_data_##index,	\
		    &i2s_stm32_config_##index, POST_KERNEL,		\
		    CONFIG_I2S_INIT_PRIORITY, &i2s_stm32_driver_api);	\
									\
static void i2s_stm32_irq_config_func_##index(struct device *dev)	\
{									\
	IRQ_CONNECT(DT_I2S_##index##_IRQ, DT_I2S_##index##_IRQ_PRI,	\
		    i2s_stm32_isr, DEVICE_GET(i2s_stm32_##index), 0);	\
	irq_enable(DT_I2S_##index##_IRQ);				\
}

#ifdef CONFIG_I2S_1
I2S_INIT(1, 2)
#endif /* CONFIG_I2S_1 */

#ifdef CONFIG_I2S_2
I2S_INIT(2, 1)
#endif /* CONFIG_I2S_2 */

#ifdef CONFIG_I2S_3
I2S_INIT(3, 1)
#endif /* CONFIG_I2S_3 */

#ifdef CONFIG_I2S_4
I2S_INIT(4, 2)
#endif /* CONFIG_I2S_4 */

#ifdef CONFIG_I2S_5
I2S_INIT(5, 2)
#endif /* CONFIG_I2S_5 */
