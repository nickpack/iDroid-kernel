#ifndef _LINUX_APPLE_FLASH_H
#define _LINUX_APPLE_FLASH_H

#include <linux/scatterlist.h>

typedef uint32_t page_t;

enum apple_nand_info
{
	NAND_NUM_CE,
	NAND_BLOCKS_PER_CE,
	NAND_PAGES_PER_CE,
	NAND_PAGES_PER_BLOCK,
	NAND_ECC_STEPS,
	NAND_BYTES_PER_SPARE,
	NAND_BANKS_PER_CE_VFL,
	NAND_BANKS_PER_CE,
	NAND_BLOCKS_PER_BANK,
	NAND_BANK_ADDRESS_SPACE,
	NAND_TOTAL_BLOCK_SPACE,

	NAND_PAGE_SIZE,
	NAND_OOB_SIZE,
	NAND_OOB_ALLOC,
};

struct apple_nand
{
	int (*read)(struct apple_nand *, int _count,
		u16 *_chips, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);

	int (*write)(struct apple_nand *, int _count,
		u16 *_chips, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);

	int (*erase)(struct apple_nand *, int _count,
		u16 *_chips, page_t *_pages);

	int (*get)(struct apple_nand *, int _info);
	int (*set)(struct apple_nand *, int _info, int _val);

	int (*is_bad)(struct apple_nand*, u16 _ce, page_t _page);
	void (*set_bad)(struct apple_nand*, u16 _ce, page_t _page);
};

int register_apple_nand(struct apple_nand*);
void remove_apple_nand(struct apple_nand*);

int apple_nand_special_page(struct apple_nand*, u16 _ce, char _page[16],
		uint8_t* _buffer, size_t _amt);
int apple_nand_read_page(struct apple_nand*, u16 _ce, page_t _page,
		uint8_t *_data, uint8_t *_oob);
int apple_nand_write_page(struct apple_nand*, u16 _ce, page_t _page,
		const uint8_t *_data, const uint8_t *_oob);
int apple_nand_erase_block(struct apple_nand*, u16 _ce, page_t _page);

struct apple_vfl
{
	struct apple_nand *nand;

	int (*read)(struct apple_vfl *, int _count,
		page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);

	int (*write)(struct apple_vfl *, int _count,
		page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);

	int (*get)(struct apple_vfl *, int _info);
	int (*set)(struct apple_vfl *, int _info, int _val);
};

int register_apple_vfl(struct apple_vfl*);
void remove_apple_vfl(struct apple_vfl*);

int apple_vfl_read_page(struct apple_vfl*, page_t _page,
		uint8_t *_data, uint8_t *_oob);
int apple_vfl_write_page(struct apple_vfl*, page_t _page,
		const uint8_t *_data, const uint8_t *_oob);

struct apple_ftl
{
	int (*read)(struct apple_ftl *, int _count, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data);

	int (*write)(struct apple_ftl *, int _count, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data);

	int (*get)(struct apple_ftl *, int _info);
	int (*set)(struct apple_ftl *, int _info, int _val);
};

int register_apple_ftl(struct apple_ftl*);
void remove_apple_ftl(struct apple_ftl*);

#endif //_LINUX_APPLE_FLASH_H
