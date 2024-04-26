/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/cap.h>
#include <zephyr/types.h>

/**
 * @brief Run the application as a CAP Initiator for unicast
 *
 * This will start scanning for and connecting to a CAP acceptor, and then attempt to setup
 * unicast streams.
 *
 * @return 0 if success, errno on failure.
 */
int cap_initiator_unicast(void);
