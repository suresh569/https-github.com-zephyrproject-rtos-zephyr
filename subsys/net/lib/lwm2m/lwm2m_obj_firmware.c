/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_lwm2m_obj_firmware
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <string.h>
#include <stdio.h>
#include <init.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"

#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0

#if defined(LWM2M_FIRMWARE_UPDATE_OBJ_SUPPORT_MULTIPLE)
#define MAX_INSTANCE_COUNT LWM2M_FIRMWARE_UPDATE_OBJ_INSTANCE_COUNT
#else
#define MAX_INSTANCE_COUNT 1
#endif

/* Firmware resource IDs */
#define FIRMWARE_PACKAGE_ID			0
#define FIRMWARE_PACKAGE_URI_ID			1
#define FIRMWARE_UPDATE_ID			2
#define FIRMWARE_STATE_ID			3
#define FIRMWARE_UPDATE_RESULT_ID		5
#define FIRMWARE_PACKAGE_NAME_ID		6
#define FIRMWARE_PACKAGE_VERSION_ID		7
#define FIRMWARE_UPDATE_PROTO_SUPPORT_ID	8
#define FIRMWARE_UPDATE_DELIV_METHOD_ID		9

#define FIRMWARE_MAX_ID				10

#define DELIVERY_METHOD_PULL_ONLY		0
#define DELIVERY_METHOD_PUSH_ONLY		1
#define DELIVERY_METHOD_BOTH			2

#define PACKAGE_URI_LEN				255

/*
 * Calculate resource instances as follows:
 * start with FIRMWARE_MAX_ID
 * subtract EXEC resources (1)
 */
#define RESOURCE_INSTANCE_COUNT	(FIRMWARE_MAX_ID - 1)

/* resource state variables */
static uint8_t update_state[MAX_INSTANCE_COUNT];
static uint8_t update_result[MAX_INSTANCE_COUNT];
static uint8_t delivery_method[MAX_INSTANCE_COUNT];
static char package_uri[MAX_INSTANCE_COUNT][PACKAGE_URI_LEN];

/* A varying number of firmware object exists */
static struct lwm2m_engine_obj firmware;
static struct lwm2m_engine_obj_field fields[] = {
	OBJ_FIELD_DATA(FIRMWARE_PACKAGE_ID, W, OPAQUE),
	OBJ_FIELD_DATA(FIRMWARE_PACKAGE_URI_ID, RW, STRING),
	OBJ_FIELD_EXECUTE(FIRMWARE_UPDATE_ID),
	OBJ_FIELD_DATA(FIRMWARE_STATE_ID, R, U8),
	OBJ_FIELD_DATA(FIRMWARE_UPDATE_RESULT_ID, R, U8),
	OBJ_FIELD_DATA(FIRMWARE_PACKAGE_NAME_ID, R_OPT, STRING),
	OBJ_FIELD_DATA(FIRMWARE_PACKAGE_VERSION_ID, R_OPT, STRING),
	OBJ_FIELD_DATA(FIRMWARE_UPDATE_PROTO_SUPPORT_ID, R_OPT, U8),
	OBJ_FIELD_DATA(FIRMWARE_UPDATE_DELIV_METHOD_ID, R, U8)
};

static struct lwm2m_engine_obj_inst inst[MAX_INSTANCE_COUNT];
static struct lwm2m_engine_res res[MAX_INSTANCE_COUNT][FIRMWARE_MAX_ID];
static struct lwm2m_engine_res_inst res_inst[MAX_INSTANCE_COUNT][RESOURCE_INSTANCE_COUNT];

static lwm2m_engine_set_data_cb_t write_cb[MAX_INSTANCE_COUNT];
static lwm2m_engine_execute_cb_t update_cb[MAX_INSTANCE_COUNT];

#ifdef CONFIG_LWM2M_FIRMWARE_UPDATE_PULL_SUPPORT
extern int lwm2m_firmware_start_transfer(uint16_t obj_inst_id, char *package_uri);
#endif

uint8_t lwm2m_firmware_get_update_state_inst(uint16_t obj_inst_id)
{
	return update_state[obj_inst_id];
}

uint8_t lwm2m_firmware_get_update_state(void)
{
	return lwm2m_firmware_get_update_state_inst(0);
}

void lwm2m_firmware_set_update_state_inst(uint16_t obj_inst_id, uint8_t state)
{
	bool error = false;
	char path[LWM2M_MAX_PATH_STR_LEN];

	/* Check LWM2M SPEC appendix E.6.1 */
	switch (state) {
	case STATE_DOWNLOADING:
		if (update_state[obj_inst_id] != STATE_IDLE) {
			error = true;
		}
		break;
	case STATE_DOWNLOADED:
		if (update_state[obj_inst_id] != STATE_DOWNLOADING &&
		    update_state[obj_inst_id] != STATE_UPDATING) {
			error = true;
		}
		break;
	case STATE_UPDATING:
		if (update_state[obj_inst_id] != STATE_DOWNLOADED) {
			error = true;
		}
		break;
	case STATE_IDLE:
		break;
	default:
		LOG_ERR("Unhandled state: %u", state);
		return;
	}

	if (error) {
		LOG_ERR("Invalid state transition: %u -> %u",
			update_state[obj_inst_id], state);
	}

	snprintk(path, sizeof(path), "%" PRIu16 "/%" PRIu16 "/%" PRIu16,
		 LWM2M_OBJECT_FIRMWARE_ID, obj_inst_id, FIRMWARE_STATE_ID);

	lwm2m_engine_set_u8(path, state);

	LOG_DBG("Update state = %d", state);
}

void lwm2m_firmware_set_update_state(uint8_t state)
{
	lwm2m_firmware_set_update_state_inst(0, state);
}

uint8_t lwm2m_firmware_get_update_result_inst(uint16_t obj_inst_id)
{
	return update_result[obj_inst_id];
}

uint8_t lwm2m_firmware_get_update_result(void)
{
	return lwm2m_firmware_get_update_result_inst(0);
}

void lwm2m_firmware_set_update_result_inst(uint16_t obj_inst_id, uint8_t result)
{
	uint8_t state;
	bool error = false;
	char path[LWM2M_MAX_PATH_STR_LEN];

	/* Check LWM2M SPEC appendix E.6.1 */
	switch (result) {
	case RESULT_DEFAULT:
		lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_IDLE);
		break;
	case RESULT_SUCCESS:
		if (update_state[obj_inst_id] != STATE_UPDATING) {
			error = true;
			state = update_state[obj_inst_id];
		}

		lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_IDLE);
		break;
	case RESULT_NO_STORAGE:
	case RESULT_OUT_OF_MEM:
	case RESULT_CONNECTION_LOST:
	case RESULT_UNSUP_FW:
	case RESULT_INVALID_URI:
	case RESULT_UNSUP_PROTO:
		if (update_state[obj_inst_id] != STATE_DOWNLOADING) {
			error = true;
			state = update_state[obj_inst_id];
		}

		lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_IDLE);
		break;
	case RESULT_INTEGRITY_FAILED:
		if (update_state[obj_inst_id] != STATE_DOWNLOADING &&
		    update_state[obj_inst_id] != STATE_UPDATING) {
			error = true;
			state = update_state[obj_inst_id];
		}

		lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_IDLE);
		break;
	case RESULT_UPDATE_FAILED:
		if (update_state[obj_inst_id] != STATE_DOWNLOADING &&
		    update_state[obj_inst_id] != STATE_UPDATING) {
			error = true;
			state = update_state[obj_inst_id];
		}

		lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_IDLE);
		break;
	default:
		LOG_ERR("Unhandled result: %u", result);
		return;
	}

	if (error) {
		LOG_ERR("Unexpected result(%u) set while state is %u",
			result, state);
	}

	snprintk(path, sizeof(path), "%" PRIu16 "/%" PRIu16 "/%" PRIu16,
		 LWM2M_OBJECT_FIRMWARE_ID, obj_inst_id, FIRMWARE_UPDATE_RESULT_ID);

	lwm2m_engine_set_u8(path, result);

	LOG_DBG("Update result = %d", result);
}

void lwm2m_firmware_set_update_result(uint8_t result)
{
	lwm2m_firmware_set_update_result_inst(0, result);
}

static int package_write_cb(uint16_t obj_inst_id, uint16_t res_id,
			    uint16_t res_inst_id, uint8_t *data, uint16_t data_len,
			    bool last_block, size_t total_size)
{
	uint8_t state;
	int ret = 0;
	lwm2m_engine_set_data_cb_t callback;

	state = lwm2m_firmware_get_update_state_inst(obj_inst_id);
	if (state == STATE_IDLE) {
		/* TODO: setup timer to check download status,
		 * make sure it fail after timeout
		 */
		lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_DOWNLOADING);
	} else if (state != STATE_DOWNLOADING) {
		if (data_len == 0U && state == STATE_DOWNLOADED) {
			/* reset to state idle and result default */
			lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_DEFAULT);
			return 0;
		}

		LOG_DBG("Cannot download: state = %d", state);
		return -EPERM;
	}

	callback = lwm2m_firmware_get_write_cb_inst(obj_inst_id);
	if (callback) {
		ret = callback(obj_inst_id, res_id, res_inst_id,
			       data, data_len, last_block, total_size);
	}

	if (ret >= 0) {
		if (last_block) {
			lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_DOWNLOADED);
		}

		return 0;
	} else if (ret == -ENOMEM) {
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_OUT_OF_MEM);
	} else if (ret == -ENOSPC) {
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_NO_STORAGE);
		/* Response 4.13 (RFC7959, section 2.9.3) */
		/* TODO: should include size1 option to indicate max size */
		ret = -EFBIG;
	} else if (ret == -EFAULT) {
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_INTEGRITY_FAILED);
	} else if (ret == -ENOMSG) {
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_UNSUP_FW);
	} else {
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_UPDATE_FAILED);
	}

	return ret;
}

static int package_uri_write_cb(uint16_t obj_inst_id, uint16_t res_id,
				uint16_t res_inst_id, uint8_t *data, uint16_t data_len,
				bool last_block, size_t total_size)
{
	LOG_DBG("PACKAGE_URI WRITE: %s", log_strdup(package_uri[obj_inst_id]));

#ifdef CONFIG_LWM2M_FIRMWARE_UPDATE_PULL_SUPPORT
	uint8_t state = lwm2m_firmware_get_update_state_inst(obj_inst_id);

	if (state == STATE_IDLE) {
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_DEFAULT);

		if (data_len > 0) {
			lwm2m_firmware_start_transfer(obj_inst_id, package_uri[obj_inst_id]);
		}
	} else if (state == STATE_DOWNLOADED && data_len == 0U) {
		/* reset to state idle and result default */
		lwm2m_firmware_set_update_result_inst(obj_inst_id, RESULT_DEFAULT);
	}

	return 0;
#else
	return -EINVAL;
#endif
}

void lwm2m_firmware_set_write_cb(lwm2m_engine_set_data_cb_t cb)
{
	lwm2m_firmware_set_write_cb_inst(0, cb);
}

lwm2m_engine_set_data_cb_t lwm2m_firmware_get_write_cb(void)
{
	return lwm2m_firmware_get_write_cb_inst(0);
}

void lwm2m_firmware_set_update_cb(lwm2m_engine_execute_cb_t cb)
{
	lwm2m_firmware_set_update_cb_inst(0, cb);
}

lwm2m_engine_execute_cb_t lwm2m_firmware_get_update_cb(void)
{
	return lwm2m_firmware_get_update_cb_inst(0);
}

void lwm2m_firmware_set_write_cb_inst(uint16_t obj_inst_id, lwm2m_engine_set_data_cb_t cb)
{
	write_cb[obj_inst_id] = cb;
}

lwm2m_engine_set_data_cb_t lwm2m_firmware_get_write_cb_inst(uint16_t obj_inst_id)
{
	return write_cb[obj_inst_id];
}

void lwm2m_firmware_set_update_cb_inst(uint16_t obj_inst_id, lwm2m_engine_execute_cb_t cb)
{
	update_cb[obj_inst_id] = cb;
}

lwm2m_engine_execute_cb_t lwm2m_firmware_get_update_cb_inst(uint16_t obj_inst_id)
{
	return update_cb[obj_inst_id];
}

static int firmware_update_cb(uint16_t obj_inst_id,
			      uint8_t *args, uint16_t args_len)
{
	lwm2m_engine_execute_cb_t callback;
	uint8_t state;
	int ret;

	state = lwm2m_firmware_get_update_state_inst(obj_inst_id);
	if (state != STATE_DOWNLOADED) {
		LOG_ERR("State other than downloaded: %d", state);
		return -EPERM;
	}

	lwm2m_firmware_set_update_state_inst(obj_inst_id, STATE_UPDATING);

	callback = lwm2m_firmware_get_update_cb_inst(obj_inst_id);
	if (callback) {
		ret = callback(obj_inst_id, args, args_len);
		if (ret < 0) {
			LOG_ERR("Failed to update firmware: %d", ret);
			lwm2m_firmware_set_update_result_inst(obj_inst_id,
				ret == -EINVAL ? RESULT_INTEGRITY_FAILED :
						 RESULT_UPDATE_FAILED);
			return 0;
		}
	}

	return 0;
}

static struct lwm2m_engine_obj_inst *firmware_create(uint16_t obj_inst_id)
{
	int i = 0, j = 0;

	init_res_instance(res_inst[obj_inst_id], ARRAY_SIZE(res_inst[obj_inst_id]));

	/* initialize instance resource data */
	INIT_OBJ_RES_OPT(FIRMWARE_PACKAGE_ID, res[obj_inst_id], i, res_inst[obj_inst_id], j, 1,
			 false, true, NULL, NULL, NULL, package_write_cb, NULL);
	INIT_OBJ_RES_LEN(FIRMWARE_PACKAGE_URI_ID, res[obj_inst_id], i, res_inst[obj_inst_id], j, 1,
		     false, true, package_uri[obj_inst_id], PACKAGE_URI_LEN, 0, NULL, NULL, NULL,
		     package_uri_write_cb, NULL);
	INIT_OBJ_RES_EXECUTE(FIRMWARE_UPDATE_ID, res[obj_inst_id], i, firmware_update_cb);
	INIT_OBJ_RES_DATA(FIRMWARE_STATE_ID, res[obj_inst_id], i, res_inst[obj_inst_id], j,
			  &(update_state[obj_inst_id]), sizeof(update_state[obj_inst_id]));
	INIT_OBJ_RES_DATA(FIRMWARE_UPDATE_RESULT_ID, res[obj_inst_id], i, res_inst[obj_inst_id], j,
			  &(update_result[obj_inst_id]), sizeof(update_result[obj_inst_id]));
	INIT_OBJ_RES_OPTDATA(FIRMWARE_PACKAGE_NAME_ID, res[obj_inst_id], i,
			     res_inst[obj_inst_id], j);
	INIT_OBJ_RES_OPTDATA(FIRMWARE_PACKAGE_VERSION_ID, res[obj_inst_id], i,
			     res_inst[obj_inst_id], j);
	INIT_OBJ_RES_MULTI_OPTDATA(FIRMWARE_UPDATE_PROTO_SUPPORT_ID, res[obj_inst_id], i,
				 res_inst[obj_inst_id], j, 1, false);
	INIT_OBJ_RES_DATA(FIRMWARE_UPDATE_DELIV_METHOD_ID, res[obj_inst_id], i,
			  res_inst[obj_inst_id], j, &(delivery_method[obj_inst_id]),
			  sizeof(delivery_method[obj_inst_id]));

	inst[obj_inst_id].resources = res[obj_inst_id];
	inst[obj_inst_id].resource_count = i;

	LOG_DBG("Create LWM2M firmware instance: %d", obj_inst_id);
	return &inst[obj_inst_id];
}

static int lwm2m_firmware_init(const struct device *dev)
{
	struct lwm2m_engine_obj_inst *obj_inst = NULL;
	int ret = 0;

	/* Set default values */
	firmware.obj_id = LWM2M_OBJECT_FIRMWARE_ID;
	firmware.version_major = FIRMWARE_VERSION_MAJOR;
	firmware.version_minor = FIRMWARE_VERSION_MINOR;
	firmware.is_core = true;
	firmware.fields = fields;
	firmware.field_count = ARRAY_SIZE(fields);
	firmware.max_instance_count = MAX_INSTANCE_COUNT;
	firmware.create_cb = firmware_create;
	lwm2m_register_obj(&firmware);


	for (int idx = 0; idx < MAX_INSTANCE_COUNT; idx++) {
		package_uri[idx][0] = '\0';

		/* Initialize state machine */
		/* TODO: should be restored from the permanent storage */
		update_state[idx] = STATE_IDLE;
		update_result[idx] = RESULT_DEFAULT;
#ifdef CONFIG_LWM2M_FIRMWARE_UPDATE_PULL_SUPPORT
		delivery_method[idx] = DELIVERY_METHOD_BOTH;
#else
		delivery_method[idx] = DELIVERY_METHOD_PUSH_ONLY;
#endif
		ret = lwm2m_create_obj_inst(LWM2M_OBJECT_FIRMWARE_ID, idx, &obj_inst);
		if (ret < 0) {
			LOG_DBG("Create LWM2M instance %d error: %d", idx, ret);
			break;
		}
	}

	return ret;
}

SYS_INIT(lwm2m_firmware_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
