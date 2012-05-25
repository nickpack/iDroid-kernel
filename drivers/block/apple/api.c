#include <linux/module.h>
#include <linux/slab.h>
#include <linux/apple_flash.h>

//
// NAND
//

int apple_nand_special_page(struct apple_nand *_nd, u16 _ce, char _page[16],
		uint8_t* _buffer, size_t _amt)
{
	int pagesz = _nd->get(_nd, NAND_PAGE_SIZE);
	int bpce = _nd->get(_nd, NAND_BLOCKS_PER_CE);
	int bpb = _nd->get(_nd, NAND_BLOCKS_PER_BANK);
	int ppb = _nd->get(_nd, NAND_PAGES_PER_BLOCK);
	int baddr = _nd->get(_nd, NAND_BANK_ADDRESS_SPACE);
	int oobsz = _nd->get(_nd, NAND_OOB_ALLOC);
	u8 *buf, *oob;
	int lowestBlock = bpce - (bpce/10);
	int block;
	int ret;
	int retval;

	int doAes = -1;
	struct cdma_aes aes;

	buf = kmalloc(pagesz, GFP_KERNEL);
	oob = kmalloc(oobsz, GFP_KERNEL);

	for(block = bpce-1; block >= lowestBlock; block--)
	{
		int page;
		int badCount = 0;

		int realBlock = (block/bpb) * baddr	+ (block % bpb);
		for(page = 0; page < ppb; page++)
		{
			if(badCount > 2)
				break;

			if((doAes = _nd->default_aes(_nd, &aes, 1)) >= 0)
				_nd->aes(_nd, &aes);

			ret = apple_nand_read_page(_nd, _ce,
					(realBlock * ppb) + page, buf, oob);

			if(ret < 0)
			{
				if(ret != -ENOENT)
					badCount++;

				continue;
			}

			/*{
				int sp = 1, i;
				for(i = 0; i < 16; i++)
				{
					if(!buf[i] || (buf[i] >= 'A' && buf[i] <= 'Z'))
						continue;
					
					sp = 0;
					break;
				}
				
				if(sp)
					print_hex_dump(KERN_INFO, "h2fmi special-page: ", DUMP_PREFIX_OFFSET, 32,
								   1, buf, 0x200, true);
								   }*/
				
			if(memcmp(buf, _page, sizeof(_page)) == 0)
			{
				if(_buffer)
				{
					size_t size = min(((size_t)((size_t*)buf)[13]), _amt);
					memcpy(_buffer, buf + 0x38, size);
				}

				retval = 0;
				goto exit;
			}
		}
	}

	printk(KERN_ERR "apple_nand: failed to find special page %s.\n", _page);
	retval = -ENOENT;

 exit:
	kfree(buf);
	kfree(oob);
	if(doAes >= 0)
		_nd->aes(_nd, NULL);
	return retval;
}
EXPORT_SYMBOL_GPL(apple_nand_special_page);

int apple_nand_read_page(struct apple_nand *_nd, u16 _ce, page_t _page,
		uint8_t *_data, uint8_t *_oob)
{
	struct scatterlist sg_buf, sg_oob;

	if(!_nd->read)
		return -EPERM;

	sg_init_one(&sg_buf, _data, _nd->get(_nd, NAND_PAGE_SIZE));
	sg_init_one(&sg_oob, _oob, _nd->get(_nd, NAND_OOB_ALLOC));

	return _nd->read(_nd, 1, &_ce, &_page, &sg_buf, _data ? 1 : 0,
					 &sg_oob, _oob ? 1 : 0);
}
EXPORT_SYMBOL_GPL(apple_nand_read_page);

int apple_nand_write_page(struct apple_nand *_nd, u16 _ce, page_t _page,
		const uint8_t *_data, const uint8_t *_oob)
{
	struct scatterlist sg_buf, sg_oob;

	if(!_nd->write)
		return -EPERM;

	sg_init_one(&sg_buf, _data, _nd->get(_nd, NAND_PAGE_SIZE));
	sg_init_one(&sg_oob, _oob, _nd->get(_nd, NAND_OOB_ALLOC));

	return _nd->write(_nd, 1, &_ce, &_page, &sg_buf, _data ? 1 : 0,
					  &sg_oob, _oob ? 1 : 0);
}
EXPORT_SYMBOL_GPL(apple_nand_write_page);

int apple_nand_erase_block(struct apple_nand *_nd, u16 _ce, page_t _page)
{
	if(!_nd->erase)
		return -EPERM;

	return _nd->erase(_nd, 1, &_ce, &_page);
}
EXPORT_SYMBOL_GPL(apple_nand_erase_block);

int apple_nand_register(struct apple_nand *_nd, struct apple_vfl *_vfl, struct device *_dev)
{
	printk(KERN_INFO "%s: %p, %p, %p\n", __func__, _nd, _vfl, _dev);

	if(_nd->vfl)
		panic("apple_nand: tried to register a NAND device more than once.\n");

	_nd->device = get_device(_dev);
	if(!_nd->device)
	{
		printk(KERN_ERR "apple_nand: tried to register dying device!\n");
		return -ENOENT;
	}

	if(_vfl->num_devices >= _vfl->max_devices)
		panic("apple_nand: tried to register more devices than we said we would!\n");

	_nd->vfl = _vfl;
	_nd->index = _vfl->num_devices;
	_vfl->num_devices++;
	_vfl->devices[_nd->index] = _nd;
	return 0;
}
EXPORT_SYMBOL_GPL(apple_nand_register);

void apple_nand_unregister(struct apple_nand *_nd)
{
	if(!_nd->vfl)
	{
		printk(KERN_WARNING "apple_nand: tried to unregister non-registered nand!\n");
		return;
	}

	// TODO: signal vfl layer!

	put_device(_nd->device);
	_nd->vfl->devices[_nd->index] = NULL;
	_nd->vfl = NULL;
}
EXPORT_SYMBOL_GPL(apple_nand_unregister);

//
// VFL
//

int apple_vfl_special_page(struct apple_vfl *_vfl,
						   u16 _ce, char _page[16],
						   uint8_t* _buffer, size_t _amt)
{
	struct apple_chip_map *chip = &_vfl->chips[_ce];
	struct apple_nand *nand = _vfl->devices[chip->bus];
	return apple_nand_special_page(nand, chip->chip,
								   _page, _buffer, _amt);
}

int apple_vfl_read_nand_pages(struct apple_vfl *_vfl,
		size_t _count, u16 *_ces, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob)
{
	// Check whether this is all on one bus, if so, just pass-through

	int ret, ok = 1, ce, bus, i;

	if(!_count)
		return 0;

	ce = _ces[0];
	bus = _vfl->chips[ce].bus;
	for(i = 1; i < _count; i++)
	{
		if(_vfl->chips[_ces[i]].bus != bus)
		{
			ok = 0;
			break;
		}
	}

	if(ok)
	{
		// only one bus, pass it along!

		struct apple_nand *nand = _vfl->devices[bus];
		u16 *chips = kmalloc(sizeof(*chips)*_count, GFP_KERNEL);
		if(!chips)
			return -ENOMEM;

		ret = nand->read(nand, _count, chips, _pages,
						 _sg_data, _sg_num_data,
						 _sg_oob, _sg_num_oob);
		kfree(chips);
		return ret;
	}
	else
	{
/*
		int nd = _vfl->num_devices;
		u16 *chips = kmalloc(sizeof(*chips)*_count*nd, GFP_KERNEL);
		page_t *pages = kmalloc(sizeof(*pages)*_count*nd, GFP_KERNEL);
		int *count = kzalloc(sizeof(*count)*nd, GFP_KERNEL);
		int num_bus = 0;

		if(!chips || !pages || !count)
		{
			kfree(chips);
			kfree(pages);
			kfree(count);
			return -ENOMEM;
		}

		for(i = 0; i < _count; i++)
		{
			int ce = _ces[i];
			int bus = _vfl->chips[ce].bus;
			int realCE = _vfl->chips[ce].chip;
			
			int idx = count[bus]++;
			chips[bus*_count + idx] = realCE;
			pages[bus*_count + idx] = _pages[i];
		}

		ret = 0;
		for(i = 0; i < nd; i++)
		{
			struct apple_nand *nand = _vfl->devices[i];

			if(!count[i])
				continue;
			
			ret = nand->read(nand, count[i],
							 &chips[i*_count],
							 &pages[i*_count],
							 
							 // ARGH, NEED TO SPLIT SGs!
		}
*/
		panic("apple-flash: SG splitting not yet implemented!\n");
	}
}
EXPORT_SYMBOL_GPL(apple_vfl_read_nand_pages);

int apple_vfl_write_nand_pages(struct apple_vfl *_vfl,
		size_t _count, u16 *_ces, page_t *_pages,
		struct scatterlist *_sg_data, size_t _sg_num_data,
		struct scatterlist *_sg_oob, size_t _sg_num_oob)
{
	// Check whether this is all on one bus, if so, just pass-through

	int ret, ok = 1, ce, bus, i;

	if(!_count)
		return 0;

	ce = _ces[0];
	bus = _vfl->chips[ce].bus;
	for(i = 1; i < _count; i++)
	{
		if(_vfl->chips[_ces[i]].bus != bus)
		{
			ok = 0;
			break;
		}
	}

	if(ok)
	{
		// only one bus, pass it along!

		struct apple_nand *nand = _vfl->devices[bus];
		u16 *chips = kmalloc(sizeof(*chips)*_count, GFP_KERNEL);
		if(!chips)
			return -ENOMEM;

		ret = nand->write(nand, _count, chips, _pages,
						 _sg_data, _sg_num_data,
						 _sg_oob, _sg_num_oob);
		kfree(chips);
		return ret;
	}
	else
		panic("apple-flash: SG splitting not implemented!\n");
}
EXPORT_SYMBOL_GPL(apple_vfl_write_nand_pages);

int apple_vfl_read_nand_page(struct apple_vfl *_vfl, u16 _ce,
		page_t _page, uint8_t *_data, uint8_t *_oob)
{
	struct scatterlist sg_buf, sg_oob;

	sg_init_one(&sg_buf, _data, _vfl->get(_vfl, NAND_PAGE_SIZE));
	sg_init_one(&sg_oob, _oob, _vfl->get(_vfl, NAND_OOB_ALLOC));

	return apple_vfl_read_nand_pages(_vfl, 1, &_ce, &_page,
					&sg_buf, _data ? 1 : 0,
					&sg_oob, _oob ? 1 : 0);
}
EXPORT_SYMBOL_GPL(apple_vfl_read_nand_page);

int apple_vfl_write_nand_page(struct apple_vfl *_vfl, u16 _ce,
        page_t _page, const uint8_t *_data, const uint8_t *_oob)
{
	struct scatterlist sg_buf, sg_oob;

	sg_init_one(&sg_buf, _data, _vfl->get(_vfl, NAND_PAGE_SIZE));
	sg_init_one(&sg_oob, _oob, _vfl->get(_vfl, NAND_OOB_ALLOC));

	return apple_vfl_write_nand_pages(_vfl, 1, &_ce,
					&_page, &sg_buf, _data ? 1 : 0,
					&sg_oob, _oob ? 1 : 0);
}
EXPORT_SYMBOL_GPL(apple_vfl_write_nand_page);

int apple_vfl_read_page(struct apple_vfl *_vfl, page_t _page,
		uint8_t *_data, uint8_t *_oob)
{
	struct scatterlist sg_buf, sg_oob;

	if(!_vfl->read)
		return -EPERM;

	sg_init_one(&sg_buf, _data, _vfl->get(_vfl, NAND_PAGE_SIZE));
	sg_init_one(&sg_oob, _oob, _vfl->get(_vfl, NAND_OOB_ALLOC));

	return _vfl->read(_vfl, 1, &_page, &sg_buf, _data ? 1 : 0,
					  &sg_oob, _oob ? 1 : 0);
}
EXPORT_SYMBOL_GPL(apple_vfl_read_page);

int apple_vfl_write_page(struct apple_vfl *_vfl, page_t _page,
		const uint8_t *_data, const uint8_t *_oob)
{
	struct scatterlist sg_buf, sg_oob;

	if(!_vfl->write)
		return -EPERM;

	sg_init_one(&sg_buf, _data, _vfl->get(_vfl, NAND_PAGE_SIZE));
	sg_init_one(&sg_oob, _oob, _vfl->get(_vfl, NAND_OOB_ALLOC));

	return _vfl->write(_vfl, 1, &_page, &sg_buf, _data ? 1 : 0,
					   &sg_oob, _oob ? 1 : 0);
}
EXPORT_SYMBOL_GPL(apple_vfl_write_page);

void apple_vfl_init(struct apple_vfl *_vfl)
{
	_vfl->devices = kzalloc(GFP_KERNEL, sizeof(*_vfl->devices)*_vfl->max_devices);

	if(!_vfl->max_chips)
		_vfl->max_chips = _vfl->max_devices*8;

	_vfl->chips = kzalloc(GFP_KERNEL, sizeof(*_vfl->chips)*_vfl->max_chips);
}
EXPORT_SYMBOL_GPL(apple_vfl_init);

int apple_vfl_register(struct apple_vfl *_vfl, enum apple_vfl_detection _detect)
{
	int i, ret;
	u8 sigbuf[264];
	u32 flags;
	struct apple_chip_map *dc = &_vfl->chips[0];

	// Setup Chip Map
	if(!_vfl->num_devices)
	{
		printk(KERN_WARNING "apple-flash: no devices!\n");
		return -ENOENT;
	}
	
	// This algo is wrong, fix it.
	for(i = 0; i < _vfl->num_devices; i++)
	{
		struct apple_nand *nand = _vfl->devices[i];
		int count = apple_nand_get(nand, NAND_NUM_CE);
		int bmap = apple_nand_get(nand, NAND_BITMAP);
		int j;
		u16 idx = 0;

		for(j = 0; j < count; j++)
		{
			struct apple_chip_map *map = &_vfl->chips[j+_vfl->num_chips];

			while((bmap & 1) == 0)
			{
				if(!bmap)
					panic("number of bits in bitmap wrong!\n");

				idx++;
				bmap >>= 1;
			}

			map->bus = i;
			map->chip = idx;
		}

		_vfl->num_chips += count;
	}

	printk(KERN_INFO "%s: detecting VFL on (%d, %u)...\n", __func__, dc->bus, dc->chip);

	// Detect VFL type
	switch(_detect)
	{
	case APPLE_VFL_OLD_STYLE:
		// TODO: implement this! -- Ricky26
		panic("apple-flash: old style VFL detection not implemented!\n");
		break;

	case APPLE_VFL_NEW_STYLE:
		ret = apple_nand_special_page(_vfl->devices[dc->bus], dc->chip,
									  "NANDDRIVERSIGN\0\0",
									  sigbuf, sizeof(sigbuf));
		break;

	default:
		ret = -EINVAL;
	};

	if(ret < 0)
	{
		printk(KERN_WARNING "apple-flash: failed to find VFL signature.\n");
		return ret;
	}

	// TODO: implement signature checking!

	printk(KERN_INFO "%s: checking signature.\n", __func__);

	flags = *(u32*)&sigbuf[4];
	if((_detect & 0x800)
	   && (!(flags & 0x10000) ||
		   ((_detect >> 10) & 1) ||
		   !((!(flags & 0x10000)) & ((_detect >> 10) & 1))))
	{
		printk(KERN_WARNING "apple-flash: metadata whitening mismatch!\n");
	}

	if(sigbuf[1] == '1')
	{
		// VSVFL
#ifdef CONFIG_BLK_DEV_APPLE_VSVFL
		ret = apple_vsvfl_detect(_vfl);
#else
		printk(KERN_ERR "apple-flash: detected VSVFL, but no support!\n");
		return -EINVAL;
#endif
	}
	else if(sigbuf[1] == '0')
	{
		// VFL
#ifdef CONFIG_BLK_DEV_APPLE_LEGACY_VFL
		ret = apple_legacy_vfl_detect(_vfl);
#else
		printk(KERN_ERR "apple-flash: detected legacy VFL, but no support!\n");
		return -EINVAL;
#endif
	}
	else
	{
		// Huh?
		printk(KERN_ERR "apple-flash: couldn't detect any VFL!\n");
		return -ENOENT;
	}

	if(ret < 0)
	{
		printk(KERN_ERR "apple-flash: failed to detect VFL.\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(apple_vfl_register);

void apple_vfl_unregister(struct apple_vfl *_vfl)
{
	kfree(_vfl->chips);
	kfree(_vfl->devices);
}
EXPORT_SYMBOL_GPL(apple_vfl_unregister);

//
// FTL
//

MODULE_AUTHOR("Ricky Taylor <rickytaylor26@gmail.com>");
MODULE_DESCRIPTION("API for Apple Mobile Device NAND.");
MODULE_LICENSE("GPL");
