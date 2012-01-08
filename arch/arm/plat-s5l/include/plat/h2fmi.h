/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __S5L_H2FMI__
#define  __S5L_H2FMI__

typedef uint32_t h2fmi_chip_id_t;

struct h2fmi_timing_info
{
	uint8_t unk1;
	uint8_t unk2;
	uint8_t unk3;
	uint8_t unk4;
	uint8_t unk5;
	uint8_t unk6;
	uint8_t unk7;
	uint8_t unk8;
};

struct h2fmi_chip_info
{
	h2fmi_chip_id_t id;
	uint16_t blocks_per_ce;
	uint16_t pages_per_block;
	uint16_t bytes_per_page;
	uint16_t bytes_per_spare;
	uint16_t unk5;
	uint16_t unk6;
	uint32_t unk7;
	uint16_t banks_per_ce;
	uint16_t unk9;

	struct h2fmi_timing_info timing;
};

struct h2fmi_smth
{
	uint8_t some_array[8];
	uint32_t symmetric_masks[];
};

struct h2fmi_platform_data
{
	int ecc_step_shift;
	int dma0, dma1;
	int pid0, pid1;

	struct h2fmi_smth *smth;
};

#endif //__S5L_H2FMI__
