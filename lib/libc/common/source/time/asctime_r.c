/*
 * Copyright (c) 2024 Meta Platforms
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <zephyr/sys/util.h>

#define DATE_STRING_BUF_SZ 26

char *asctime_r(const struct tm *tp, char *buf)
{
	static const char wday_str[7][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
	};
	static const char mon_str[12][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	};

	if ((tp == NULL) || (buf == NULL) || (tp->tm_wday >= ARRAY_SIZE(wday_str)) ||
	    (tp->tm_mon >= ARRAY_SIZE(mon_str))) {
		return NULL;
	}

	snprintf(buf, DATE_STRING_BUF_SZ, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n", wday_str[tp->tm_wday],
		 mon_str[tp->tm_mon], tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec,
		 1900 + tp->tm_year);

	return buf;
}

char *asctime(const struct tm *tp)
{
	static char buf[DATE_STRING_BUF_SZ];

	return asctime_r(tp, buf);
}
