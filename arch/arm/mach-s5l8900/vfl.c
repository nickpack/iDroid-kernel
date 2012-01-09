#include <linux/io.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <ftl/nand.h>
#include <ftl/vfl.h>

#define LOG printk
#define LOGDBG(format, ...)

typedef struct VFLCxt {
	u32 usnInc;				// 0x000
	u16 FTLCtrlBlock[3];			// 0x004
	u8 unk1[2];				// 0x00A
	u32 usnDec;				// 0x00C
	u16 activecxtblock;			// 0x010
	u16 nextcxtpage;				// 0x012
	u8 unk2[2];				// 0x014
	u16 field_16;				// 0x016
	u16 field_18;				// 0x018
	u16 numReservedBlocks;			// 0x01A
	u16 reservedBlockPoolStart;		// 0x01C
	u16 totalReservedBlocks;			// 0x01E
	u16 reservedBlockPoolMap[0x334];		// 0x020
	u8 badBlockTable[0x11a];			// 0x688
	u16 VFLCxtBlock[4];			// 0x7A2
	u16 remappingScheduledStart;		// 0x7AA
	u8 unk3[0x4C];				// 0x7AC
	u32 checksum1;				// 0x7F8
	u32 checksum2;				// 0x7FC
} VFLCxt;

#define FTL_ID_V1 0x43303033
#define FTL_ID_V2 0x43303034
#define FTL_ID_V3 0x43303035

//#define IPHONE_DEBUG

// Shared Counters

extern VFLData1Type VFLData1;

// Shared configuration

NANDData* NANDGeometry;
NANDFTLData* FTLData;

// Global Buffers

static u8* PageBuffer;
static u8* SpareBuffer;

static VFLCxt* pstVFLCxt = NULL;
static u8* pstBBTArea = NULL;
static u32* ScatteredPageNumberBuffer = NULL;
static u16* ScatteredBankNumberBuffer = NULL;
static int curVFLusnInc = 0;

// Prototypes

static bool findDeviceInfoBBT(int bank, void* deviceInfoBBT)
{
	int lowestBlock = NANDGeometry->blocksPerBank - (NANDGeometry->blocksPerBank / 10);
	int block;

	for(block = NANDGeometry->blocksPerBank - 1; block >= lowestBlock; block--) {
		int page;
		int badBlockCount = 0;
		for(page = 0; page < NANDGeometry->pagesPerBlock; page++) {
			int ret;

			if(badBlockCount > 2) {
				LOGDBG("ftl: findDeviceInfoBBT - too many bad pages, skipping block %d\n", block);
				break;
			}

			ret = nand_read_alternate_ecc(bank, (block * NANDGeometry->pagesPerBlock) + page, PageBuffer);
			if(ret != 0) {
				if(ret == 1) {
					LOGDBG("ftl: findDeviceInfoBBT - found 'badBlock' on bank %d, page %d\n", (block * NANDGeometry->pagesPerBlock) + page);
					badBlockCount++;
				}

				LOGDBG("ftl: findDeviceInfoBBT - skipping bank %d, page %d\n", (block * NANDGeometry->pagesPerBlock) + page);
				continue;
			}

			if(memcmp(PageBuffer, "DEVICEINFOBBT\0\0\0", 16) == 0) {
				if(deviceInfoBBT) {
					memcpy(deviceInfoBBT, PageBuffer + 0x38, *((u32*)(PageBuffer + 0x34)));
				}

				return true;
			} else {
				LOGDBG("ftl: did not find signature on bank %d, page %d\n", (block * NANDGeometry->pagesPerBlock) + page);
			}
		}
	}

	return false;
}

// pageBuffer and spareBuffer are represented by single BUF struct within Whimory
static bool nand_read_vfl_cxt_page(int bank, int block, int page, u8* pageBuffer, u8* spareBuffer) {
	int i;
	for(i = 0; i < 8; i++) {
		if(nand_read(bank, (block * NANDGeometry->pagesPerBlock) + page + i, pageBuffer, spareBuffer, true, true) == 0) {
			SpareData* spareData = (SpareData*) spareBuffer;
			if(spareData->type2 == 0 && spareData->type1 == 0x80)
				return true;
		}
	}
	return false;
}

static void vfl_checksum(void* data, int size, u32* a, u32* b) {
	int i;
	u32* buffer = (u32*) data;
	u32 x = 0;
	u32 y = 0;
	for(i = 0; i < (size / 4); i++) {
		x += buffer[i];
		y ^= buffer[i];
	}

	*a = x + 0xAABBCCDD;
	*b = y ^ 0xAABBCCDD;
}

static bool vfl_gen_checksum(int bank) {
	vfl_checksum(&pstVFLCxt[bank], (u32)&pstVFLCxt[bank].checksum1 - (u32)&pstVFLCxt[bank], &pstVFLCxt[bank].checksum1, &pstVFLCxt[bank].checksum2);
	return false;
}

static bool vfl_check_checksum(int bank) {
	u32 checksum1;
	u32 checksum2;
	static int counter = 0;

	counter++;

	vfl_checksum(&pstVFLCxt[bank], (u32)&pstVFLCxt[bank].checksum1 - (u32)&pstVFLCxt[bank], &checksum1, &checksum2);

	// Yeah, this looks fail, but this is actually the logic they use
	if(checksum1 == pstVFLCxt[bank].checksum1)
		return true;

	if(checksum2 != pstVFLCxt[bank].checksum2)
		return true;

	return false;
}


static int vfl_store_cxt(int bank)
{
	int i;
	int good;
	SpareData* spareData = (SpareData*) SpareBuffer;

	--pstVFLCxt[bank].usnDec;
	pstVFLCxt[bank].usnInc = ++curVFLusnInc;
	pstVFLCxt[bank].nextcxtpage += 8;
	vfl_gen_checksum(bank);

	memset(spareData, 0xFF, NANDGeometry->bytesPerSpare);
	spareData->meta.usnDec = pstVFLCxt[bank].usnDec;
	spareData->type2 = 0;
	spareData->type1 = 0x80;

	for(i = 0; i < 8; ++i)
	{
		u32 index = pstVFLCxt[bank].activecxtblock;
		u32 block = pstVFLCxt[bank].VFLCxtBlock[index];
		u32 page = block * NANDGeometry->pagesPerBlock;
		page += pstVFLCxt[bank].nextcxtpage - 8 + i;
		nand_write(bank, page, (u8*) &pstVFLCxt[bank], (u8*) spareData, true);
	}

	good = 0;
	for(i = 0; i < 8; ++i)
	{
		u32 index = pstVFLCxt[bank].activecxtblock;
		u32 block = pstVFLCxt[bank].VFLCxtBlock[index];
		u32 page = block * NANDGeometry->pagesPerBlock;
		page += pstVFLCxt[bank].nextcxtpage - 8 + i;
		if(nand_read(bank, page, PageBuffer, (u8*) spareData, true, true) != 0)
			continue;

		if(memcmp(PageBuffer, &pstVFLCxt[bank], sizeof(VFLCxt)) != 0)
			continue;

		if(spareData->type2 == 0 && spareData->type1 == 0x80)
			++good;

	}

	return (good > 3) ? 0 : -1;
}

static int vfl_commit_cxt(int bank)
{
	u32 cur;
	u32 block;

	if((pstVFLCxt[bank].nextcxtpage + 8) <= NANDGeometry->pagesPerBlock)
		if(vfl_store_cxt(bank) == 0)
			return 0;

	cur = pstVFLCxt[bank].activecxtblock;
	block = cur;

	while(true)
	{
		int i;

		block = (block + 1) % 4;
		if(block == cur)
			break;

		// try to erase 4 times
		for(i = 0; i < 4; ++i)
		{
			if(nand_erase(bank, pstVFLCxt[bank].VFLCxtBlock[block]) == 0)
				break;
		}

		if(i == 4)
			continue;

		pstVFLCxt[bank].activecxtblock = block;
		pstVFLCxt[bank].nextcxtpage = 0;
		if(vfl_store_cxt(bank) == 0)
			return 0;
	}

	LOG("ftl: failed to commit VFL context!\n");
	return -1;
}

static void virtual_page_number_to_virtual_address(u32 dwVpn, u16* virtualBank, u16* virtualBlock, u16* virtualPage) {
	*virtualBank = dwVpn % NANDGeometry->banksTotal;
	*virtualBlock = dwVpn / NANDGeometry->pagesPerSuBlk;
	*virtualPage = (dwVpn / NANDGeometry->banksTotal) % NANDGeometry->pagesPerBlock;
}

// badBlockTable is a bit array with 8 virtual blocks in one bit entry
static bool isGoodBlock(u8* badBlockTable, u16 virtualBlock) {
	int index = virtualBlock/8;
	return ((badBlockTable[index / 8] >> (7 - (index % 8))) & 0x1) == 0x1;
}

static u16 virtual_block_to_physical_block(u16 virtualBank, u16 virtualBlock) {
	int pwDesPbn;

	if(isGoodBlock(pstVFLCxt[virtualBank].badBlockTable, virtualBlock))
		return virtualBlock;

	for(pwDesPbn = 0; pwDesPbn < pstVFLCxt[virtualBank].numReservedBlocks; pwDesPbn++) {
		if(pstVFLCxt[virtualBank].reservedBlockPoolMap[pwDesPbn] == virtualBlock) {
			if(pwDesPbn >= NANDGeometry->blocksPerBank) {
				LOG("ftl: Destination physical block for remapping is greater than number of blocks per bank!");
			}
			return pstVFLCxt[virtualBank].reservedBlockPoolStart + pwDesPbn;
		}
	}

	return virtualBlock;
}

static bool vfl_check_remap_scheduled(int bank, u16 block)
{
	int i;
	for(i = 0x333; i > 0 && i > pstVFLCxt[bank].remappingScheduledStart; --i)
	{
		if(pstVFLCxt[bank].reservedBlockPoolMap[i] == block)
			return true;
	}

	return false;
}

static bool vfl_schedule_block_for_remap(int bank, u16 block)
{
	if(vfl_check_remap_scheduled(bank, block))
		return true;

	LOG("ftl: attempting to schedule bank %d, block %d for remap!\n", bank, block);

	// don't do anything for right now to avoid consequences for false positives
	return false;

	if(pstVFLCxt[bank].remappingScheduledStart == (pstVFLCxt[bank].numReservedBlocks + 10))
	{
		// oh crap, we only have 10 free spares left. back out now.
		return false;
	}

	// stick this into the list
	--pstVFLCxt[bank].remappingScheduledStart;
	pstVFLCxt[bank].reservedBlockPoolMap[pstVFLCxt[bank].remappingScheduledStart] = block;
	vfl_gen_checksum(bank);

	return vfl_commit_cxt(bank);
}

static void vfl_set_good_block(int bank, u16 block, int isGood)
{
	int index = block / 8;
	u8 bit = 1 << (7 - (index % 8));
	if(isGood)
		pstVFLCxt[bank].badBlockTable[index / 8] |= bit;
	else
		pstVFLCxt[bank].badBlockTable[index / 8] &= ~bit;
}

static u16 vfl_remap_block(int bank, u16 block)
{
	u16 newBlock = 0;
	int newBlockIdx;
	int i;

	if(bank >= NANDGeometry->banksTotal || block >= NANDGeometry->blocksPerBank)
		return 0;

	LOG("ftl: attempting to remap bank %d, block %d\n", bank, block);
	return 0;

	// find a reserved block that is not being used
	for(i = 0; i < pstVFLCxt[bank].totalReservedBlocks; ++i)
	{
		if(pstVFLCxt[bank].reservedBlockPoolMap[i] == 0)
		{
			newBlock = pstVFLCxt[bank].reservedBlockPoolStart + i;
			newBlockIdx = i;
			break;
		}
	}

	// none found
	if(newBlock == 0)
		return 0;

	// try to erase newly allocated reserved block nine times
	for(i = 0; i < 9; ++i)
	{
		if(nand_erase(bank, newBlock) == 0)
			break;
	}

	for(i = 0; i < newBlockIdx; ++i)
	{
		// mark any reserved block previously remapped for this block as bad
		if(pstVFLCxt[bank].reservedBlockPoolMap[i] == block)
			pstVFLCxt[bank].reservedBlockPoolMap[i] = 0xFFFF;
	}

	pstVFLCxt[bank].reservedBlockPoolMap[newBlockIdx] = block;
	++pstVFLCxt[bank].numReservedBlocks;
	vfl_set_good_block(bank, block, false);

	return newBlock;
}

static void vfl_mark_remap_done(int bank, u16 block)
{
	u16 start;
	u16 lastscheduled;
	int i;

	LOG("ftl: attempt to mark remap as done for bank %d, block %d\n", bank, block);
	return;

	start = pstVFLCxt[bank].remappingScheduledStart;
	lastscheduled = pstVFLCxt[bank].reservedBlockPoolMap[start];
	for (i = 0x333; i > 0 && i > start; i--)
	{
		if (pstVFLCxt[bank].reservedBlockPoolMap[i] == block)
		{
			// replace the done entry with the last one
			if(i != start && i != 0x333)
				pstVFLCxt[bank].reservedBlockPoolMap[i] = lastscheduled;

			++pstVFLCxt[bank].remappingScheduledStart;
			return;
		}
	}
}

static bool hasDeviceInfoBBT(void)
{
	int bank;
	bool good = true;
	for(bank = 0; bank < NANDGeometry->banksTotal; bank++) {
		good = findDeviceInfoBBT(bank, NULL);
		if(!good)
			return false;
	}

	return good;
}

int VFL_Erase(u16 block) {
	u16 physicalBlock;
	int ret;
	int bank;
	int i;

	block = block + FTLData->field_4;

	for(bank = 0; bank < NANDGeometry->banksTotal; ++bank) {
		if(vfl_check_remap_scheduled(bank, block))
		{
			vfl_remap_block(bank, block);
			vfl_mark_remap_done(bank, block);
			vfl_commit_cxt(bank);
		}

		physicalBlock = virtual_block_to_physical_block(bank, block);

		for(i = 0; i < 3; ++i)
		{
			ret = nand_erase(bank, physicalBlock);
			if(ret == 0)
				break;
		}

		if(ret) {
			LOG("ftl: block erase failed for bank %d, block %d\n", bank, block);
			// FIXME: properly handle this
			return ret;
		}
	}

	return 0;
}

int VFL_Read(u32 virtualPageNumber, u8* buffer, u8* spare, bool empty_ok)
{
	u16 virtualBank;
	u16 virtualBlock;
	u16 virtualPage;
	u16 physicalBlock;
	u32 dwVpn;

	int page;
	int ret;

	VFLData1.field_8++;
	VFLData1.field_20++;

	dwVpn = virtualPageNumber + (NANDGeometry->pagesPerSuBlk * FTLData->field_4);
	if(dwVpn >= NANDGeometry->pagesTotal) {
		LOG("ftl: dwVpn overflow: %d\n", dwVpn);
		return -EINVAL;
	}

	if(dwVpn < NANDGeometry->pagesPerSuBlk) {
		LOG("ftl: dwVpn underflow: %d\n", dwVpn);
	}

	virtual_page_number_to_virtual_address(dwVpn, &virtualBank, &virtualBlock, &virtualPage);
	physicalBlock = virtual_block_to_physical_block(virtualBank, virtualBlock);

	page = physicalBlock * NANDGeometry->pagesPerBlock + virtualPage;

#ifdef IPHONE_DEBUG
	LOG("ftl: vfl_read: vpn: %u, bank %d, page %u\n", virtualPageNumber, virtualBank, page);
#endif

	ret = nand_read(virtualBank, page, buffer, spare, true, true);

	if(!empty_ok && ret == ERROR_EMPTYBLOCK)
	{
		ret = -EIO;
	}

	if(ret == -EINVAL || ret == -EIO) {
		nand_bank_reset(virtualBank, 100);
		ret = nand_read(virtualBank, page, buffer, spare, true, true);
		if(!empty_ok && ret == ERROR_EMPTYBLOCK) {
			return -EIO;
		}

		if(ret == -EINVAL || ret == -EIO)
			return ret;
	}

	if(ret == ERROR_EMPTYBLOCK) {
		if(spare) {
			memset(spare, 0xFF, sizeof(SpareData));
		}
	}

	return ret;
}

int VFL_Write(u32 virtualPageNumber, u8* buffer, u8* spare)
{
	u16 virtualBank;
	u16 virtualBlock;
	u16 virtualPage;
	u16 physicalBlock;

	u32 dwVpn;

	int page;
	int ret;

	dwVpn = virtualPageNumber + (NANDGeometry->pagesPerSuBlk * FTLData->field_4);
	if(dwVpn >= NANDGeometry->pagesTotal) {
		LOG("ftl: dwVpn overflow: %d\n", dwVpn);
		return -EINVAL;
	}

	if(dwVpn < NANDGeometry->pagesPerSuBlk) {
		LOG("ftl: dwVpn underflow: %d\n", dwVpn);
	}

	virtual_page_number_to_virtual_address(dwVpn, &virtualBank, &virtualBlock, &virtualPage);
	physicalBlock = virtual_block_to_physical_block(virtualBank, virtualBlock);

	page = physicalBlock * NANDGeometry->pagesPerBlock + virtualPage;

#ifdef IPHONE_DEBUG
	LOG("ftl: vfl_write: vpn: %u, bank %d, page %u\n", virtualPageNumber, virtualBank, page);
#endif

		ret = nand_read(virtualBank, page, PageBuffer, SpareBuffer, true, true);
	if(ret != ERROR_EMPTYBLOCK)
	{
		LOG("ftl: WTF trying to write to a non-blank page! vpn = %u bank = %d page = %u\r\n", virtualPageNumber, virtualBank, page);
		return -1;
	}

	ret = nand_write(virtualBank, page, buffer, spare, true);
	if(ret == 0)
		return 0;

	++pstVFLCxt[virtualBank].field_16;
	vfl_gen_checksum(virtualBank);
	vfl_schedule_block_for_remap(virtualBank, virtualBlock);

	return -1;
}

bool VFL_ReadMultiplePagesInVb(int logicalBlock, int logicalPage, int count, u8* main, SpareData* spare)
{
	int i;
	int currentPage = logicalPage;
	for(i = 0; i < count; i++) {
		int ret = VFL_Read((logicalBlock * NANDGeometry->pagesPerSuBlk) + currentPage, main + (NANDGeometry->bytesPerPage * i), (u8*) &spare[i], true);
		currentPage++;
		if(ret != 0)
			return false;
	}
	return true;
}

bool VFL_ReadScatteredPagesInVb(u32* virtualPageNumber, int count, u8* main, SpareData* spare)
{
	int i;
	int ret;

	VFLData1.field_8 += count;
	VFLData1.field_20++;

	for(i = 0; i < count; i++) {
		u32 dwVpn = virtualPageNumber[i] + (NANDGeometry->pagesPerSuBlk * FTLData->field_4);
		u16 virtualBlock;
		u16 virtualPage;
		u16 physicalBlock;

		virtual_page_number_to_virtual_address(dwVpn, &ScatteredBankNumberBuffer[i], &virtualBlock, &virtualPage);
		physicalBlock = virtual_block_to_physical_block(ScatteredBankNumberBuffer[i], virtualBlock);
		ScatteredPageNumberBuffer[i] = physicalBlock * NANDGeometry->pagesPerBlock + virtualPage;
#ifdef IPHONE_DEBUG
		LOG("ftl: vfl_read (scattered): vpn: %u, bank  %d, page %u\n", virtualPageNumber[i], ScatteredBankNumberBuffer[i], ScatteredPageNumberBuffer[i]);
#endif
	}

	ret = nand_read_multiple(ScatteredBankNumberBuffer, ScatteredPageNumberBuffer, main, spare, count);

	if(ret != 0)
		return false;
	else
		return true;
}

u16* VFL_GetFTLCtrlBlock(void)
{
	int bank = 0;
	int max = 0;
	u16* FTLCtrlBlock = NULL;
	for(bank = 0; bank < NANDGeometry->banksTotal; bank++)
	{
		int cur = pstVFLCxt[bank].usnInc;
		if(max <= cur) {
			max = cur;
			FTLCtrlBlock = pstVFLCxt[bank].FTLCtrlBlock;
		}
	}

	return FTLCtrlBlock;
}

int VFL_Open(void)
{
	void* FTLCtrlBlock;
	u16 buffer[3];

	int bank = 0;
	for(bank = 0; bank < NANDGeometry->banksTotal; bank++) {
		VFLCxt* curVFLCxt;
		int i;
		int minUsn;
		int VFLCxtIdx;
		int last;
		int page;

		if(!findDeviceInfoBBT(bank, pstBBTArea)) {
			LOG("ftl: findDeviceInfoBBT failed\n");
			return -1;
		}

		if(bank >= NANDGeometry->banksTotal) {
			return -1;
		}

		curVFLCxt = &pstVFLCxt[bank];

		// Any VFLCxt page will contain an up-to-date list of all blocks used to store VFLCxt pages. Find any such
		// page in the system area.

		for(i = 1; i < FTLData->sysSuBlks; i++) {
			// so pstBBTArea is a bit array of some sort
			if(!(pstBBTArea[i / 8] & (1 << (i  & 0x7))))
				continue;

			if(nand_read_vfl_cxt_page(bank, i, 0, PageBuffer, SpareBuffer) == true) {
				memcpy(curVFLCxt->VFLCxtBlock, ((VFLCxt*)PageBuffer)->VFLCxtBlock, sizeof(curVFLCxt->VFLCxtBlock));
				break;
			}
		}

		if(i == FTLData->sysSuBlks) {
			LOG("ftl: cannot find readable VFLCxtBlock\n");
			return -1;
		}

		// Since VFLCxtBlock is a ringbuffer, if blockA.page0.spare.usnDec < blockB.page0.usnDec, then for any page a
	        // in blockA and any page b in blockB, a.spare.usNDec < b.spare.usnDec. Therefore, to begin finding the
		// page/VFLCxt with the lowest usnDec, we should just look at the first page of each block in the ring.
		minUsn = 0xFFFFFFFF;
		VFLCxtIdx = 4;
		for(i = 0; i < 4; i++) {
			SpareData* spareData;
			u16 block = curVFLCxt->VFLCxtBlock[i];
			if(block == 0xFFFF)
				continue;

			if(nand_read_vfl_cxt_page(bank, block, 0, PageBuffer, SpareBuffer) != true)
				continue;

			spareData = (SpareData*) SpareBuffer;
			if(spareData->meta.usnDec > 0 && spareData->meta.usnDec <= minUsn) {
				minUsn = spareData->meta.usnDec;
				VFLCxtIdx = i;
			}
		}

		if(VFLCxtIdx == 4) {
			LOG("ftl: cannot find readable VFLCxtBlock index in spares\n");
			return -1;
		}

		// VFLCxts are stored in the block such that they are duplicated 8 times. Therefore, we only need to
		// read every 8th page, and nand_read_vfl_cxt_page will try the 7 subsequent pages if the first was
		// no good. The last non-blank page will have the lowest spare.usnDec and highest usnInc for VFLCxt
		// in all the land (and is the newest).
		last = 0;
		for(page = 8; page < NANDGeometry->pagesPerBlock; page += 8) {
			if(nand_read_vfl_cxt_page(bank, curVFLCxt->VFLCxtBlock[VFLCxtIdx], page, PageBuffer, SpareBuffer) == false) {
				break;
			}

			last = page;
		}

		if(nand_read_vfl_cxt_page(bank, curVFLCxt->VFLCxtBlock[VFLCxtIdx], last, PageBuffer, SpareBuffer) == false) {
			LOG("ftl: cannot find readable VFLCxt\n");
			return -1;
		}

		// Aha, so the upshot is that this finds the VFLCxt and copies it into pstVFLCxt
		memcpy(&pstVFLCxt[bank], PageBuffer, sizeof(VFLCxt));

		// This is the newest VFLCxt across all banks
		if(curVFLCxt->usnInc >= curVFLusnInc) {
			curVFLusnInc = curVFLCxt->usnInc;
		}

		// Verify the checksum
		if(vfl_check_checksum(bank) == false) {
			LOG("ftl: VFLCxt has bad checksum\n");
			return -1;
		}
	}

	// retrieve the FTL control blocks from the latest VFL across all banks.
	FTLCtrlBlock = VFL_GetFTLCtrlBlock();

	// Need a buffer because eventually we'll copy over the source
	memcpy(buffer, FTLCtrlBlock, sizeof(buffer));

	// Then we update the VFLCxts on every bank with that information.
	for(bank = 0; bank < NANDGeometry->banksTotal; bank++) {
		memcpy(pstVFLCxt[bank].FTLCtrlBlock, buffer, sizeof(buffer));
		vfl_gen_checksum(bank);
	}

	return 0;
}

int VFL_Init(void)
{
	nand_setup();

	NANDGeometry = nand_get_geometry();
	FTLData = nand_get_ftl_data();

	memset(&VFLData1, 0, sizeof(VFLData1));
	if(pstVFLCxt == NULL) {
		pstVFLCxt = kmalloc(NANDGeometry->banksTotal * sizeof(VFLCxt), GFP_KERNEL | GFP_DMA);
		if(pstVFLCxt == NULL)
			return -1;
	}

	if(pstBBTArea == NULL) {
		pstBBTArea = (u8*) kmalloc((NANDGeometry->blocksPerBank + 7) / 8, GFP_KERNEL | GFP_DMA);
		if(pstBBTArea == NULL)
			return -1;
	}

	if(ScatteredPageNumberBuffer == NULL && ScatteredBankNumberBuffer == NULL) {
		ScatteredPageNumberBuffer = (u32*) kmalloc(NANDGeometry->pagesPerSuBlk * 4, GFP_KERNEL | GFP_DMA);
		ScatteredBankNumberBuffer = (u16*) kmalloc(NANDGeometry->pagesPerSuBlk * 4, GFP_KERNEL | GFP_DMA);
		if(ScatteredPageNumberBuffer == NULL || ScatteredBankNumberBuffer == NULL)
			return -1;
	}

	PageBuffer = (u8*) kmalloc(NANDGeometry->bytesPerPage, GFP_KERNEL | GFP_DMA);
	SpareBuffer = (u8*) kmalloc(NANDGeometry->bytesPerSpare, GFP_KERNEL | GFP_DMA);

	curVFLusnInc = 0;

	return 0;
}

int VFL_StoreFTLCtrlBlock(u16* ftlctrlblock)
{
	int bank;
	for(bank = 0; bank < NANDGeometry->banksTotal; bank++)
		memcpy(pstVFLCxt[bank].FTLCtrlBlock, ftlctrlblock, sizeof(pstVFLCxt[bank].FTLCtrlBlock));

	// pick a semi-random bank to commit
	return vfl_commit_cxt(curVFLusnInc % NANDGeometry->banksTotal);
}

int VFL_Verify(void)
{
	int i;
	int foundSignature = false;

	LOGDBG("ftl: Attempting to read %d pages from first block of first bank.\n", NANDGeometry->pagesPerBlock);
	for(i = 0; i < NANDGeometry->pagesPerBlock; i++) {
		int ret;
		if((ret = nand_read_alternate_ecc(0, i, PageBuffer)) == 0) {
			u32 id = *((u32*) PageBuffer);
			if(id == FTL_ID_V1 || id == FTL_ID_V2 || id == FTL_ID_V3) {
				LOG("ftl: Found production format: %x\n", id);
				foundSignature = true;
				break;
			} else {
				LOGDBG("ftl: Found non-matching signature: %x\n", ((u32*) PageBuffer));
			}
		} else {
			LOGDBG("ftl: page %d of first bank is unreadable: %x!\n", i, ret);
		}
	}

	if(!foundSignature || !hasDeviceInfoBBT()) {
		LOG("ftl: no signature or production format.\n");
		return -1;
	}

	return 0;
}

