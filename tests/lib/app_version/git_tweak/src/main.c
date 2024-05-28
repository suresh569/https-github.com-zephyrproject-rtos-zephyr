/*
 * Copyright (c) 2024 Embeint Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/ztest.h>

#include "app_version.h"

ZTEST(app_version, test_git_tweak)
{
	/* From VERSION */
	zassert_equal(2, APP_VERSION_MAJOR);
	zassert_equal(3, APP_VERSION_MINOR);
	zassert_equal(4, APP_PATCHLEVEL);
	zassert_mem_equal("2.3.4-somestring", APP_VERSION_STRING, sizeof(APP_VERSION_STRING));

	/* Mostly validating that compilation succeeded.
	 * Also validate that we weren't given some default value.
	 */
	zassert_not_equal(0, APP_TWEAK);
	zassert_not_equal(UINT32_MAX, APP_TWEAK);
}

ZTEST_SUITE(app_version, NULL, NULL, NULL, NULL, NULL);
