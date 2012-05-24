#ifndef _LINUX_APPLE_FLASH_H
#define _LINUX_APPLE_FLASH_H

#include <linux/scatterlist.h>
#include <linux/device.h>
#include <plat/cdma.h>

typedef uint32_t page_t;

enum apple_nand_info
{
	NAND_NUM_CE,
	NAND_BITMAP,

	NAND_BLOCKS_PER_CE,
	NAND_PAGES_PER_CE,
	NAND_PAGES_PER_BLOCK,
	NAND_ECC_STEPS,
	NAND_ECC_BITS,
	NAND_BYTES_PER_SPARE,
	NAND_BANKS_PER_CE_VFL,
	NAND_BANKS_PER_CE,
	NAND_BLOCKS_PER_BANK,
	NAND_BLOCK_ADDRESS_SPACE,
	NAND_BANK_ADDRESS_SPACE,
	NAND_TOTAL_BLOCK_SPACE,

	NAND_PAGE_SIZE,
	NAND_OOB_SIZE,
	NAND_OOB_ALLOC,

	VFL_PAGES_PER_BLOCK,
	VFL_USABLE_BLOCKS_PER_BANK,
	VFL_FTL_TYPE,
};

enum apple_vfl_detection
{
	APPLE_VFL_OLD_STYLE = 0x64,
	APPLE_VFL_NEW_STYLE = 0x65,
};

struct apple_chip_map
{
	int bus;
	u16 chip;
};

struct apple_vfl;

struct apple_nand
{
	struct device *device;
	struct apple_vfl *vfl;
	int index;

	int (*detect)(struct apple_nand *, uint8_t *_buf, size_t _size);

	int (*default_aes)(struct apple_nand *, struct cdma_aes *, int _decrypt);
	int (*aes)(struct apple_nand *, struct cdma_aes *);

	int (*read)(struct apple_nand *, size_t _count,
		u16 *_chips, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);

	int (*write)(struct apple_nand *, size_t _count,
		u16 *_chips, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);

	int (*erase)(struct apple_nand *, size_t _count,
		u16 *_chips, page_t *_pages);

	int (*get)(struct apple_nand *, int _info);
	int (*set)(struct apple_nand *, int _info, int _val);

	int (*is_bad)(struct apple_nand*, u16 _ce, page_t _page);
	void (*set_bad)(struct apple_nand*, u16 _ce, page_t _page);
};

static inline int apple_nand_get(struct apple_nand *_nd, int _id)
{
	if(!_nd)
		return 0;

	return _nd->get(_nd, _id);
}

static inline int apple_nand_set(struct apple_nand *_nd, int _id, int _val)
{
	if(!_nd)
		return -EINVAL;

	return _nd->set(_nd, _id, _val);
}

extern int apple_nand_special_page(struct apple_nand*, u16 _ce, char _page[16],
		uint8_t* _buffer, size_t _amt);
extern int apple_nand_read_page(struct apple_nand*, u16 _ce, page_t _page,
		uint8_t *_data, uint8_t *_oob);
extern int apple_nand_write_page(struct apple_nand*, u16 _ce, page_t _page,
	 	const uint8_t *_data, const uint8_t *_oob);
extern int apple_nand_erase_block(struct apple_nand*, u16 _ce, page_t _page);

extern int apple_nand_register(struct apple_nand*, struct apple_vfl*, struct device*);
extern void apple_nand_unregister(struct apple_nand*);

struct apple_vfl
{
	int num_devices;
	int max_devices;
	struct apple_nand **devices;
	int num_chips;
	int max_chips;
	struct apple_chip_map *chips;

	void *private;

	void (*cleanup)(struct apple_vfl*);

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

extern void apple_vfl_init(struct apple_vfl*);
extern int apple_vfl_register(struct apple_vfl*, enum apple_vfl_detection _detect);
extern void apple_vfl_unregister(struct apple_vfl*);

extern int apple_vfl_special_page(struct apple_vfl*, u16 _ce, char _page[16],
		uint8_t* _buffer, size_t _amt);
extern int apple_vfl_read_nand_pages(struct apple_vfl*,
		size_t _count, u16 *_ces, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);
extern int apple_vfl_write_nand_pages(struct apple_vfl*,
		size_t _count, u16 *_ces, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob);
extern int apple_vfl_read_nand_page(struct apple_vfl*, u16 _ce,
		page_t _page, uint8_t *_data, uint8_t *_oob);
extern int apple_vfl_write_nand_page(struct apple_vfl*, u16 _ce,
        page_t _page, const uint8_t *_data, const uint8_t *_oob);
extern int apple_vfl_erase_nand_block(struct apple_vfl*, u16 _ce,
	    page_t _page);
extern int apple_vfl_read_page(struct apple_vfl*, page_t _page,
		uint8_t *_data, uint8_t *_oob);
extern int apple_vfl_write_page(struct apple_vfl*, page_t _page,
		const uint8_t *_data, const uint8_t *_oob);

extern int apple_legacy_vfl_detect(struct apple_vfl *_vfl);
extern int apple_vsvfl_detect(struct apple_vfl *_vfl);

static inline int apple_vfl_get(struct apple_vfl *_nd, int _id)
{
	if(!_nd)
		return 0;

	return _nd->get(_nd, _id);
}

static inline int apple_vfl_set(struct apple_vfl *_nd, int _id, int _val)
{
	if(!_nd)
		return -EINVAL;

	return _nd->set(_nd, _id, _val);
}

struct apple_ftl
{
	struct apple_vfl *vfl;
	void *private;

	int (*read)(struct apple_ftl *, size_t _count, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data);

	int (*write)(struct apple_ftl *, size_t _count, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data);

	int (*get)(struct apple_ftl *, int _info);
	int (*set)(struct apple_ftl *, int _info, int _val);
};

#endif //_LINUX_APPLE_FLASH_H
