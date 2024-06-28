/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2023 Oxide Computer Company
 */

/*
 * JESD402-1A Temperature Ranges
 */

#include <libjedec.h>
#include <sys/sysmacros.h>

typedef struct {
	libjedec_temp_range_t	ltm_temp;
	int32_t			ltm_min;
	int32_t			ltm_max;
} libjedec_temp_map_t;

static const libjedec_temp_map_t libjedec_temp_map[] = {
	{ JEDEC_TEMP_CASE_A1T, -40, 125 },
	{ JEDEC_TEMP_CASE_A2T, -40, 105 },
	{ JEDEC_TEMP_CASE_A3T, -40, 85 },
	{ JEDEC_TEMP_CASE_IT, -40, 95 },
	{ JEDEC_TEMP_CASE_ET, -25, 105 },
	{ JEDEC_TEMP_CASE_ST, -25, 85 },
	{ JEDEC_TEMP_CASE_XT, 0, 95 },
	{ JEDEC_TEMP_CASE_NT, 0, 85 },
	{ JEDEC_TEMP_CASE_RT, 0, 45 },
	{ JEDEC_TEMP_AMB_CT, 0, 70 },
	{ JEDEC_TEMP_AMB_IOT, -40, 85 },
	{ JEDEC_TEMP_AMB_IPT, -40, 105 },
	{ JEDEC_TEMP_AMB_IXT, -40, 125 },
	{ JEDEC_TEMP_AMB_AO3T, -40, 85 },
	{ JEDEC_TEMP_AMB_AO2T, -40, 105 },
	{ JEDEC_TEMP_AMB_AO1T, -40, 125 },
	{ JEDEC_TEMP_STOR_2, -55, 125 },
	{ JEDEC_TEMP_STOR_1B, -55, 100 },
	{ JEDEC_TEMP_STOR_1A, -40, 105 },
	{ JEDEC_TEMP_STOR_ST, -40, 85 }
};

boolean_t
libjedec_temp_range(libjedec_temp_range_t temp, int32_t *min, int32_t *max)
{
	for (size_t i = 0; i < ARRAY_SIZE(libjedec_temp_map); i++) {
		if (temp == libjedec_temp_map[i].ltm_temp) {
			*min = libjedec_temp_map[i].ltm_min;
			*max = libjedec_temp_map[i].ltm_max;
			return (B_TRUE);
		}
	}

	return (B_FALSE);
}
