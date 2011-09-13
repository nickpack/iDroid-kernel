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

struct h2fmi_chipid
{
	uint32_t chipID;
	uint32_t zero;
};

struct h2fmi_chip_info
{
	struct h2fmi_chipid chipID;
	uint16_t blocks_per_ce;
	uint16_t pages_per_block;
	uint16_t bytes_per_page;
	uint16_t bytes_per_spare;
	uint16_t unk5;
	uint16_t unk6;
	uint32_t unk7;
	uint16_t banks_per_ce;
	uint16_t unk9;
};

struct h2fmi_board_id
{
	uint8_t num_busses;
	uint8_t num_symmetric;
	struct h2fmi_chipid chipID;
	uint8_t chip1_count;
	struct h2fmi_chipid chipID2;
	uint8_t chip2_count;
};

struct h2fmi_board_info
{
	struct h2fmi_board_id board_id;
	uint32_t vendor_type;
};

struct h2fmi_timing_info
{
	struct h2fmi_board_id board_id;
	uint8_t unk1;
	uint8_t unk2;
	uint8_t unk3;
	uint8_t unk4;
	uint8_t unk5;
	uint8_t unk6;
	uint8_t unk7;
	uint8_t unk8;
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

	int (*complete_board_id)(int _idx, u8 _chip_id[8], int _bitmap, struct h2fmi_board_id *_id);

	struct h2fmi_chip_info *(*find_chip)(u8 _chip_id[8]);
	struct h2fmi_board_info *(*find_board)(struct h2fmi_board_id *_id);
	struct h2fmi_timing_info *(*find_timing)(struct h2fmi_board_id *_id);
};

#endif //__S5L_H2FMI__
