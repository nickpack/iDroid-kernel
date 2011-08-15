/**
 *
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <plat/sdhci.h>

static const char *sdio0_clocks[] = {
	"sdio",
	NULL,
	NULL,
	NULL,
};

static struct s3c_sdhci_platdata sdio0_pdata = {
};
