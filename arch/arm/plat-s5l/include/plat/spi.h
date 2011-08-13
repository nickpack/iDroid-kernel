/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  S5L_SPI_H
#define  S5L_SPI_H

struct s5l_spi_info
{
	int pin_cs;
	unsigned int num_cs;

	int bus_num;
	
	void (*gpio_setup)(struct s5l_spi_info *spi, int enable);
	void (*set_cs)(struct s5l_spi_info *spi, int cs, int pol);
};

#include <mach/spi.h>

#endif //S5L_SPI_H
