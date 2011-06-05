#include <linux/io.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <ftl/vfl.h>
#include <ftl/ftl.h>
#include <mach/iphone-clock.h>

#define LOG printk
#define LOGDBG(format, ...)

typedef struct FTLCxtLog {
	u32 usn;					// 0x0
	u16 wVbn;					// 0x4
	u16 wLbn;					// 0x6
	u16* wPageOffsets;				// 0x8
	u16 pagesUsed;				// 0xC
	u16 pagesCurrent;				// 0xE
	u32 isSequential;				// 0x10
} FTLCxtLog;

typedef struct FTLCxtElement2 {
	u16 field_0;				// 0x0
	u16 field_2;				// 0x2
} FTLCxtElement2;

typedef struct FTLCxt {
	u32 usnDec;				// 0x0
	u32 nextblockusn;				// 0x4
	u16 wNumOfFreeVb;				// 0x8
	u16 nextFreeIdx;				// 0xA
	u16 swapCounter;				// 0xC
	u16 awFreeVb[20];				// 0xE
	u16 field_36;				// 0x36
	u32 pages_for_pawMapTable[18];		// 0x38
	u32 pages_for_pawEraseCounterTable[36];	// 0x80
	u32 pages_for_wPageOffsets[34];		// 0x110
	u16* pawMapTable;				// 0x198
	u16* pawEraseCounterTable;			// 0x19C
	u16* wPageOffsets;				// 0x1A0
	FTLCxtLog pLog[18];				// 0x1A4
	u32 eraseCounterPagesDirty;		// 0x30C
	u16 unk3;					// 0x310
	u16 FTLCtrlBlock[3];			// 0x312
	u32 FTLCtrlPage;				// 0x318
	u32 clean;					// 0x31C
	u32 pages_for_pawReadCounterTable[36];	// 0x320
	u16* pawReadCounterTable;			// 0x3B0
	FTLCxtElement2 elements2[5];			// 0x3B4
	u32 field_3C8;				// 0x3C8
	u32 totalReadCount;			// 0x3CC
	u32 page_for_FTLCountsTable;		// 0x3D0
	u32 hasFTLCountsTable;			// 0x3D4, set to -1 if yes
	u8 field_3D8[0x420];			// 0x3D8
	u32 versionLower;				// 0x7F8
	u32 versionUpper;				// 0x7FC
} FTLCxt;

typedef struct FTLCountsTableType {
	u64 totalPagesWritten;		// 0x0
	u64 totalPagesRead;		// 0x8
	u64 totalWrites;			// 0x10
	u64 totalReads;			// 0x18
	u64 compactScatteredCount;		// 0x20
	u64 copyMergeWhileFullCount;	// 0x28
	u64 copyMergeWhileNotFullCount;	// 0x30
	u64 simpleMergeCount;		// 0x38
	u64 blockSwapCount;		// 0x40
	u64 ftlRestoresCount;		// 0x48
	u64 field_50;
} FTLCountsTableType;

typedef enum FTLStruct {
	FTLCountsTableSID = 0x1000200
} FTLStruct;

typedef enum VFLStruct {
	VFLData1SID = 0x2000200,
	VFLData5SID = 0x2000500
} VFLStruct;

#define FTL_ID_V1 0x43303033
#define FTL_ID_V2 0x43303034
#define FTL_ID_V3 0x43303035

// Shared counters

VFLData1Type VFLData1;
static uint8_t VFLData5[0xF8];

#ifdef FTL_PROFILE
u64 TotalWriteTime;
u64 TotalSyncTime;
extern u64 Time_wait_for_ecc_interrupt;
extern u64 Time_wait_for_ready;
extern u64 Time_wait_for_address_done;
extern u64 Time_wait_for_command_done;
extern u64 Time_wait_for_transfer_done;
extern u64 Time_wait_for_nand_bank_ready;
extern u64 Time_nand_write;
extern u64 Time_iphone_dma_finish;
#endif

// Global Buffers

extern NANDData* NANDGeometry;
extern NANDFTLData* FTLData;

static u8* PageBuffer;
static SpareData* FTLSpareBuffer;

static FTLCountsTableType FTLCountsTable;
static FTLCxt* pstFTLCxt;
static FTLCxt* FTLCxtBuffer;
static u32* ScatteredVirtualPageNumberBuffer;
static bool CleanFreeVb;

// Synchronization

static DEFINE_MUTEX(ftl_mutex);

// Prototypes

static bool ftl_merge(FTLCxtLog* pLog);
static bool ftl_open_read_counter_tables(void);

static int FTL_Init(void) {
	int i;
	memset(&FTLCountsTable, 0, 0x58);

	PageBuffer = (u8*) kmalloc(NANDGeometry->bytesPerPage, GFP_KERNEL | GFP_DMA);
	pstFTLCxt = FTLCxtBuffer = (FTLCxt*) kmalloc(sizeof(FTLCxt), GFP_KERNEL | GFP_DMA);
	if(pstFTLCxt == NULL)
		return -1;
	memset(pstFTLCxt->field_3D8, 0, sizeof(pstFTLCxt->field_3D8));

	pstFTLCxt->eraseCounterPagesDirty = 0;

	pstFTLCxt->pawMapTable = (u16*) kmalloc(NANDGeometry->userSuBlksTotal * sizeof(u16), GFP_KERNEL);
	pstFTLCxt->wPageOffsets = (u16*) kmalloc((NANDGeometry->pagesPerSuBlk * 18) * sizeof(u16), GFP_KERNEL);
	pstFTLCxt->pawEraseCounterTable = (u16*) kmalloc((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16), GFP_KERNEL);
	pstFTLCxt->pawReadCounterTable = (u16*) kmalloc((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16), GFP_KERNEL);

	FTLSpareBuffer = (SpareData*) kmalloc(NANDGeometry->pagesPerSuBlk * sizeof(SpareData), GFP_KERNEL | GFP_DMA);

	ScatteredVirtualPageNumberBuffer = (u32*) kmalloc(NANDGeometry->pagesPerSuBlk * sizeof(u32*), GFP_KERNEL);

	if(!pstFTLCxt->pawMapTable || !pstFTLCxt->wPageOffsets || !pstFTLCxt->pawEraseCounterTable || !FTLCxtBuffer->pawReadCounterTable || ! FTLSpareBuffer || !ScatteredVirtualPageNumberBuffer)
		return -1;

	for(i = 0; i < 18; i++) {
		pstFTLCxt->pLog[i].wPageOffsets = pstFTLCxt->wPageOffsets + (i * NANDGeometry->pagesPerSuBlk);
		memset(pstFTLCxt->pLog[i].wPageOffsets, 0xFF, NANDGeometry->pagesPerSuBlk * 2);
		pstFTLCxt->pLog[i].isSequential = 1;
		pstFTLCxt->pLog[i].pagesUsed = 0;
		pstFTLCxt->pLog[i].pagesCurrent = 0;
	}

	return 0;
}

static void FTL_64bit_sum(u64* src, u64* dest, int size) {
	int i;
	for(i = 0; i < size / sizeof(u64); i++) {
		dest[i] += src[i];
	}
}

static bool ftl_set_free_vb(u16 block)
{
	// get to the end of the ring buffer
	int nextFreeVb = (pstFTLCxt->nextFreeIdx + pstFTLCxt->wNumOfFreeVb) % 20;
	++pstFTLCxt->wNumOfFreeVb;

	++pstFTLCxt->pawEraseCounterTable[block];
	pstFTLCxt->pawReadCounterTable[block] = 0;

	if(VFL_Erase(block) != 0)
	{
		LOG("ftl: failed to release a virtual block from the pool\n");
		return false;
	}

	pstFTLCxt->awFreeVb[nextFreeVb] = block;

	return true;
}

static bool ftl_get_free_vb(u16* block)
{
	int i;

	int chosenVbIdx = 20;
	int curFreeIdx = pstFTLCxt->nextFreeIdx;
	u16 smallestEC = 0xFFFF;
	u16 chosenVb;

	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; ++i)
	{
		if(pstFTLCxt->awFreeVb[curFreeIdx] != 0xFFFF)
		{
			if(pstFTLCxt->pawEraseCounterTable[pstFTLCxt->awFreeVb[curFreeIdx]] < smallestEC)
			{
				smallestEC = pstFTLCxt->pawEraseCounterTable[pstFTLCxt->awFreeVb[curFreeIdx]];
				chosenVbIdx = curFreeIdx;
			}
		}
		curFreeIdx = (curFreeIdx + 1) % 20;
	}

	if(chosenVbIdx > 19)
	{
		LOG("ftl: could not find a free vb!\n");
		return false;
	}

	chosenVb = pstFTLCxt->awFreeVb[chosenVbIdx];

	if(chosenVbIdx != pstFTLCxt->nextFreeIdx)
	{
		// swap
		pstFTLCxt->awFreeVb[chosenVbIdx] = pstFTLCxt->awFreeVb[pstFTLCxt->nextFreeIdx];
		pstFTLCxt->awFreeVb[pstFTLCxt->nextFreeIdx] = chosenVb;
	}

	if(chosenVb > (NANDGeometry->userSuBlksTotal + 23))
	{
		LOG("ftl: invalid free vb\n");
		return false;
	}

	--pstFTLCxt->wNumOfFreeVb;
	if(pstFTLCxt->wNumOfFreeVb > 19)
	{
		LOG("ftl: invalid freeVbn\n");
		return false;
	}

	if(pstFTLCxt->nextFreeIdx > 19)
	{
		LOG("ftl: invalid vbListTail\n");
		return false;
	}

	// increment cursor
	pstFTLCxt->nextFreeIdx = (pstFTLCxt->nextFreeIdx + 1) % 20;

	*block = chosenVb;
	return true;
}

static bool ftl_next_ctrl_page(void)
{
	int i;
	int blockIdx;

	++pstFTLCxt->FTLCtrlPage;
	if((pstFTLCxt->FTLCtrlPage % NANDGeometry->pagesPerSuBlk) != 0)
	{
		--pstFTLCxt->usnDec;
		return true;
	}

	// find old block to swap out

	for(i = 0; i < 3; ++i)
	{
		if(((pstFTLCxt->FTLCtrlBlock[i] + 1) * NANDGeometry->pagesPerSuBlk) == pstFTLCxt->FTLCtrlPage)
			break;
	}

	blockIdx = (i + 1) % 3;

	if((pstFTLCxt->eraseCounterPagesDirty % 30) > 2)
	{
		LOG("ftl: reusing ctrl block at: %d\n", pstFTLCxt->FTLCtrlBlock[blockIdx]);

		++pstFTLCxt->eraseCounterPagesDirty;
		++pstFTLCxt->pawEraseCounterTable[pstFTLCxt->FTLCtrlBlock[blockIdx]];
		++pstFTLCxt->pawReadCounterTable[pstFTLCxt->FTLCtrlBlock[blockIdx]];
		if(VFL_Erase(pstFTLCxt->FTLCtrlBlock[blockIdx]) != 0)
		{
			LOG("ftl: next_ctrl_page failed to erase and markEC(0x%X)\n", pstFTLCxt->FTLCtrlBlock[blockIdx]);
			return false;
		}

		pstFTLCxt->FTLCtrlPage = pstFTLCxt->FTLCtrlBlock[blockIdx] * NANDGeometry->pagesPerSuBlk;
		--pstFTLCxt->usnDec;

		return true;
	} else
	{
		u16 newBlock;
		u16 oldBlock;

		++pstFTLCxt->eraseCounterPagesDirty;

		if(!ftl_get_free_vb(&newBlock))
		{
			LOG("ftl: next_ctrl_page failed to get free VB\n");
			return false;
		}

		LOG("ftl: allocated new ctrl block at: %d\n", newBlock);

		oldBlock = pstFTLCxt->FTLCtrlBlock[blockIdx];

		pstFTLCxt->FTLCtrlBlock[blockIdx] = newBlock;
		pstFTLCxt->FTLCtrlPage = newBlock * NANDGeometry->pagesPerSuBlk;

		if(!ftl_set_free_vb(oldBlock))
		{
			LOG("ftl: next_ctrl_page failed to set free VB\n");
			return false;
		}


		if(VFL_StoreFTLCtrlBlock(pstFTLCxt->FTLCtrlBlock) == 0)
		{
			--pstFTLCxt->usnDec;
			return true;
		} else
		{
			LOG("ftl: next_ctrl_page failed to store FTLCtrlBlock info in VFL\n");
			return false;
		}
	}
}

// Return whether the block is sequential and also the highest USN of pages
// found in that block. Assumption: for any pages p_i, p_j in a block where
// i < j, usn(p_i) <= usn(p_j)
static bool determine_block_type(u16 block, u32* highest_usn)
{
	bool isSequential = true;
	u8* pageBuffer = (u8*) kmalloc(NANDGeometry->bytesPerPage, GFP_KERNEL | GFP_DMA);
	SpareData* spareData = (SpareData*) kmalloc(NANDGeometry->bytesPerSpare, GFP_KERNEL | GFP_DMA);

	u32 max = 0;
	int page;
	for(page = NANDGeometry->pagesPerSuBlk - 1; page >= 0; --page)
	{
		int ret = VFL_Read(block * NANDGeometry->pagesPerSuBlk + page, pageBuffer, (u8*) spareData, true);
		if(ret != 0)
			continue;

		if(spareData->user.usn > max)
			max = spareData->user.usn;

		if((spareData->user.logicalPageNumber % NANDGeometry->pagesPerSuBlk) != page)
			isSequential = false;
	}

	kfree(spareData);
	kfree(pageBuffer);

	*highest_usn = max;

	return isSequential;
}

// Assumptions: same conditions that FTL_Open would have been called in.
static bool FTL_Restore(void)
{
	u16* blockMap = (u16*) kmalloc((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16), GFP_KERNEL);
	u8* isEmpty = (u8*) kmalloc((NANDGeometry->userSuBlksTotal + 23) * sizeof(u8), GFP_KERNEL);
	u8* nonSequential = (u8*) kmalloc((NANDGeometry->userSuBlksTotal + 23) * sizeof(u8), GFP_KERNEL);
	u32* usnA = (u32*) kmalloc(sizeof(u32) * NANDGeometry->pagesPerSuBlk, GFP_KERNEL);
	u32* usnB = (u32*) kmalloc(sizeof(u32) * NANDGeometry->pagesPerSuBlk, GFP_KERNEL);
	u32* lpnA = (u32*) kmalloc(sizeof(u32) * NANDGeometry->pagesPerSuBlk, GFP_KERNEL);
	u32* lpnB = (u32*) kmalloc(sizeof(u32) * NANDGeometry->pagesPerSuBlk, GFP_KERNEL);

	u16* awFreeVb = &pstFTLCxt->awFreeVb[0];
	u16* pawMapTable = pstFTLCxt->pawMapTable;
	u16* pawEraseCounterTable = pstFTLCxt->pawEraseCounterTable;
	void* pawReadCounterTable = pstFTLCxt->pawReadCounterTable;
	u16* wPageOffsets = pstFTLCxt->wPageOffsets;
	FTLCxtLog* pLog = &pstFTLCxt->pLog[0];

	int i;
	int block;
	void* FTLCtrlBlock;
	u32 ftlCtrlBlock;
	u32 minUsnDec;
	int ftlCxtFound;
	int page;
	u32 highest_usn;
	int numLogs;

	LOG("ftl: restore searching for latest FTL context...\n");

	// Step 0, find and load the last readable FTLCxt, so we can have a base erase counter data and
	// goodies. It is not really mandatory though; we could just make a new one.

	if((FTLCtrlBlock = VFL_GetFTLCtrlBlock()) == NULL)
	{
		LOG("ftl: restore could not get FTL control blocks from VFL!\n");
		goto error_release;
	}

	memcpy(pstFTLCxt->FTLCtrlBlock, FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));

	ftlCtrlBlock = 0xffff;
	minUsnDec = 0xffffffff;
	ftlCxtFound = false;
	for(i = 0; i < sizeof(pstFTLCxt->FTLCtrlBlock)/sizeof(u16); ++i)
	{
		u32 blockUSNDec;

		// read the first page of the block
		int ret = VFL_Read(NANDGeometry->pagesPerSuBlk * pstFTLCxt->FTLCtrlBlock[i], PageBuffer, (u8*) FTLSpareBuffer, true);
		if(ret != 0)
			continue;	// this block errored out!

		// 0x43 is the lowest type of FTL control data. Apparently 0x4F would be the highest type.
		if((FTLSpareBuffer->type1 - 0x43) > 0xC)
			continue;	// this block doesn't have FTL data in it! Try the next one

		if(ftlCtrlBlock != 0xffff && FTLSpareBuffer->meta.usnDec >= minUsnDec)
			continue;	// we've seen a newer FTLCxtBlock before

		// this is the latest so far
		ftlCtrlBlock = pstFTLCxt->FTLCtrlBlock[i];

		blockUSNDec = FTLSpareBuffer->meta.usnDec;

		for(page = NANDGeometry->pagesPerSuBlk - 1; page > 0; page--)
		{
			ret = VFL_Read(NANDGeometry->pagesPerSuBlk * ftlCtrlBlock + page, PageBuffer, (u8*) FTLSpareBuffer, true);
			if(ret == 1) {
				continue;
			} else if(ret == 0 && FTLSpareBuffer->type1 == 0x43) { // 43 is FTLCxtBlock
				minUsnDec = blockUSNDec;
				memcpy(pstFTLCxt, PageBuffer, sizeof(FTLCxt));

				// we just overwrote our good FTLCtrlBlock info, fill it in again.
				memcpy(pstFTLCxt->FTLCtrlBlock, FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));
				ftlCxtFound = true;
				break;
			}
		}
	}

	if(!ftlCxtFound)
	{
		LOG("ftl: restore could not find ANY FTL contexts!\n");
		goto error_release;
	}

	LOG("ftl: restore found useable FTL context with usnDec = 0x%x\n", pstFTLCxt->usnDec);

	// Reset the pointers
	pstFTLCxt->pawMapTable = pawMapTable;
	pstFTLCxt->pawEraseCounterTable = pawEraseCounterTable;
	pstFTLCxt->pawReadCounterTable = pawReadCounterTable;
	pstFTLCxt->wPageOffsets = wPageOffsets;

	// Clean out now almost certainly invalid values

	memset(&FTLCountsTable, 0, 0x58);

	pstFTLCxt->clean = 0;

	for(i = 0; i < 18; ++i)
	{
		pLog[i].wPageOffsets = pstFTLCxt->wPageOffsets + (i * NANDGeometry->pagesPerSuBlk);
		memset(pLog[i].wPageOffsets, 0xFF, NANDGeometry->pagesPerSuBlk * sizeof(u16));
		pLog[i].isSequential = 1;
		pLog[i].pagesUsed = 0;
		pLog[i].pagesCurrent = 0;
		pLog[i].wVbn = 0xFFFF;
	}

	for(i = 0; i < 20; ++i)
		awFreeVb[i] = 0xFFFF;

	pstFTLCxt->nextFreeIdx = 0;
	pstFTLCxt->wNumOfFreeVb = 0;

	// Read back the counter tables... those are still useful.
	if(!ftl_open_read_counter_tables())
	{
		LOG("ftl: restore could not read back the counter tables from the FTL context!\n");
		goto error_release;
	}

	++FTLCountsTable.ftlRestoresCount;

	// next time we commit, do it on a fresh block
	block = pstFTLCxt->FTLCtrlPage / NANDGeometry->pagesPerSuBlk;
	pstFTLCxt->FTLCtrlPage = (block * NANDGeometry->pagesPerSuBlk) + NANDGeometry->pagesPerSuBlk - 1;

	numLogs = 0;

	// Step one, create an overview of which virtual blocks have pages belonging to which logical blocks.
	// Mark any blocks discovered to be empty. Also to save time in the next step, if a block is proven
	// to be non-sequential, mark it as such.

	for(block = 0; block < (NANDGeometry->userSuBlksTotal + 23); ++block)
	{
		if((block % 1000) == 0)
		{
			LOG("ftl: restore scanning virtual blocks %d - %d\n", block,
					block + ((((NANDGeometry->userSuBlksTotal + 23) - block) > 1000) ? 999 : ((NANDGeometry->userSuBlksTotal + 23) - block - 1)));
		}

		blockMap[block] = 0xFFFF;
		isEmpty[block] = 1;
		nonSequential[block] = 0;

		for(page = 0; page < NANDGeometry->pagesPerSuBlk; ++page)
		{
			int ret = VFL_Read(block * NANDGeometry->pagesPerSuBlk + page, PageBuffer, (u8*) FTLSpareBuffer, true);

			if(ret == ERROR_EMPTYBLOCK)
				continue;

			isEmpty[block] = 0;

			if(ret != 0)
				continue;

			if(FTLSpareBuffer->type1 >= 0x43 && FTLSpareBuffer->type1 <= 0x4F)
				break;

			// wtf is this? well, we'll just count it as empty
			if(FTLSpareBuffer->type1 != 0x40 && FTLSpareBuffer->type1 != 0x41)
				continue;

			if((FTLSpareBuffer->user.logicalPageNumber % NANDGeometry->pagesPerSuBlk) != page)
				nonSequential[block] = 1;

			blockMap[block] = FTLSpareBuffer->user.logicalPageNumber / NANDGeometry->pagesPerSuBlk;
			break;
		}
	}

	// Step two, make sure each logical block has a mapping to virtual block. If more than one virtual
	// block contain pages to a logical block, pick which one is a mapping block and which one is a
	// log block based on whether the entries are sequential and which ones have the highest USN. If
	// no virtual block for a logical block is found, then it must have been empty so we select an
	// empty block to use as its virtual block.

	LOG("ftl: restore creating mapping table...\n");

	for(block = 0; block < NANDGeometry->userSuBlksTotal; ++block)
	{
		u16 mapCandidate = 0xFFFF;
		u32 mapCandidateUSN = 0xFFFFFFFF;
		u16 logCandidate = 0xFFFF;
		u32 logCandidateUSN = 0xFFFFFFFF;
		u16 candidate;

		if((block % 1000) == 0)
		{
			LOG("ftl: restore scanning logical blocks %d - %d\r\n", block,
					block + (((NANDGeometry->userSuBlksTotal - block) > 1000) ? 999 : (NANDGeometry->userSuBlksTotal - block - 1)));
		}

		for(candidate = 0; candidate < (NANDGeometry->userSuBlksTotal + 23); ++candidate)
		{
			u32 candidateUSN;
			bool origMCSeq;
			bool newMCSeq;
			u16 newLCandidate;
			u32 newLCandidateUSN;

			if(blockMap[candidate] != block)
				continue;

			if(nonSequential[candidate])
			{
				if(logCandidate == 0xFFFF)
				{
					logCandidate = candidate;
					continue;
				}
				if(logCandidateUSN == 0xFFFFFFFF)
					determine_block_type(logCandidate, &logCandidateUSN);

				determine_block_type(candidate, &candidateUSN);

				if(logCandidateUSN < candidateUSN)
				{
					logCandidate = candidate;
					logCandidateUSN = candidateUSN;
				}
			} else if(mapCandidate == 0xFFFF)
			{
				mapCandidate = candidate;
				continue;
			}

			origMCSeq = true;
			if(mapCandidateUSN == 0xFFFFFFFF)
				origMCSeq = determine_block_type(mapCandidate, &mapCandidateUSN);

			newMCSeq = determine_block_type(candidate, &candidateUSN);

			if(origMCSeq && newMCSeq)
			{
				if(mapCandidateUSN > candidateUSN)
				{
					newLCandidate = candidate;
					newLCandidateUSN = candidateUSN;
				} else
				{
					newLCandidate = mapCandidate;
					newLCandidateUSN = mapCandidateUSN;
					mapCandidate = candidate;
					mapCandidateUSN = candidateUSN;
				}
			} else if(origMCSeq && !newMCSeq)
			{
				newLCandidate = candidate;
				newLCandidateUSN = candidateUSN;
			} else if(!origMCSeq && newMCSeq)
			{
				newLCandidate = mapCandidate;
				newLCandidateUSN = mapCandidateUSN;
				mapCandidate = candidate;
				mapCandidateUSN = candidateUSN;
			} else
			{
				// neither is sequential! Then we'll unset the map candidate and decide
				// the newest one to be the potential new log candidate.
				if(mapCandidateUSN > candidateUSN)
				{
					newLCandidate = mapCandidate;
					newLCandidateUSN = mapCandidateUSN;
				} else
				{
					newLCandidate = candidate;
					newLCandidateUSN = candidateUSN;
				}
				mapCandidate = 0xFFFF;
				mapCandidateUSN = 0xFFFFFFFF;
			}

			if(logCandidate == 0xFFFF)
			{
				logCandidate = newLCandidate;
				logCandidateUSN = newLCandidateUSN;
				continue;
			}

			if(logCandidateUSN == 0xFFFFFFFF)
				determine_block_type(logCandidate, &logCandidateUSN);

			if(logCandidateUSN < newLCandidateUSN)
			{
				logCandidate = newLCandidate;
				logCandidateUSN = newLCandidateUSN;
			}
		}

		if(mapCandidate == 0xFFFF)
		{
			for(candidate = 0; candidate < (NANDGeometry->userSuBlksTotal + 23); ++candidate)
			{
				if(isEmpty[candidate])
				{
					mapCandidate = candidate;
					isEmpty[candidate] = 0;
					break;
				}
			}

			if(mapCandidate == 0xFFFF)
			{
				LOG("ftl: restore failed, didn't find enough empty blocks to pair with orphan logical blocks.\n");
				goto error_release;
			}
		}

		pawMapTable[block] = mapCandidate;
		blockMap[mapCandidate] = 0;

		if(logCandidate != 0xFFFF)
		{
			pLog[numLogs].wLbn = block;
			pLog[numLogs].wVbn = logCandidate;
			++numLogs;

			blockMap[logCandidate] = 0;
		}
	}

	// Step three
	// At this point, pawMapTable ought to be correct. We can also assert that all blocks that were originally
	// either in pawMapTable or pLog, we have put in pawMapTable or pLog. The rest are either the three control
	// blocks, or the free vbs. Therefore, we look for blocks that were not marked as pawMapTable or pLog and
	// also not a control block and mark them as free vbs.

	LOG("ftl: restore determing free vbs...\n");

	for(block = 0; block < (NANDGeometry->userSuBlksTotal + 23); ++block)
	{
		if(blockMap[block] == 0)
			continue;

		for(i = 0; i < 3; ++i)
		{
			if(block == pstFTLCxt->FTLCtrlBlock[i])
				break;
		}

		if(i < 3)
			continue;

		awFreeVb[pstFTLCxt->wNumOfFreeVb++] = block;
	}

	LOG("ftl: restore wNumOfFreeVb = %d, number of log vbs = %d\n",
			pstFTLCxt->wNumOfFreeVb, numLogs);

	// Now we do a consistency check. The total number of virtual blocks accessible by
	// our FTL is the number of user superblocks + 3 control blocks + 20 blocks divided
	// between free blocks and log blocks. It can get down to 3 free blocks and 17 log
	// blocks (defined by the size of the log block array, and explicitly guarded by
	// ftl_prepare_log), or up to 20 free blocks and 0 log blocks (there is space for
	// 20 free vbs). That counts up to the + 23 number you see everywhere. Therefore,
	// the number of free vbs and the number of log blocks used must equal 20.
	if((pstFTLCxt->wNumOfFreeVb + numLogs) != 20)
	{
		LOG("ftl: restore failed, we are missing pool blocks!\n");
		goto error_release;
	}

	// Step four. Now for those blocks that have log entries, we must decide how to
	// populate the page offsets table, i.e. divide the logical block between these
	// two blocks. Assumption: if block L is the log block for block B, pick any
	// page u from L and v from B -- usn(u) > usn(v). This is because after the
	// map block is written, any further write to it will hit the log block for it
	// and increment the USN.

	// This keeps track of the highest USN for all log blocks.
	highest_usn = 0;

	for(i = 0; i < numLogs; ++i)
	{
		// Since we always store the highest USN block as the map in step two
		// to ensure we always end up with the two highest USN blocks, we
		// could have the ordering swapped. Figure out the correct one.

		u16 blockA = pawMapTable[pLog[i].wLbn];
		u16 blockB = pLog[i].wVbn;
		bool aSequential = true;
		bool bSequential = true;
		u32 aHighestUSN = 0;
		u32 bHighestUSN = 0;
		u32* mapBlockUSN;
		u32* mapBlockLPN;
		u32* logBlockUSN;
		u32* logBlockLPN;

		// Populate information about these two blocks

		for(page = NANDGeometry->pagesPerSuBlk - 1; page >= 0; --page)
		{
			int ret = VFL_Read((blockA * NANDGeometry->pagesPerSuBlk) + page, PageBuffer, (u8*) FTLSpareBuffer, true);
			if(ret == ERROR_EMPTYBLOCK)
				usnA[page] = 0;
			else
		{
				usnA[page] = FTLSpareBuffer->user.usn;
				lpnA[page] = FTLSpareBuffer->user.logicalPageNumber;

				if(usnA[page] > aHighestUSN)
					aHighestUSN = usnA[page];

				if((FTLSpareBuffer->user.logicalPageNumber % NANDGeometry->pagesPerSuBlk) != page)
					aSequential = false;
			}
		}

		for(page = NANDGeometry->pagesPerSuBlk - 1; page >= 0; --page)
			{
			int ret = VFL_Read((blockB * NANDGeometry->pagesPerSuBlk) + page, PageBuffer, (u8*) FTLSpareBuffer, true);
			if(ret == ERROR_EMPTYBLOCK)
				usnB[page] = 0;
			else
			{
				usnB[page] = FTLSpareBuffer->user.usn;
				lpnB[page] = FTLSpareBuffer->user.logicalPageNumber;

				if(usnB[page] > bHighestUSN)
					bHighestUSN = usnB[page];

				if((FTLSpareBuffer->user.logicalPageNumber % NANDGeometry->pagesPerSuBlk) != page)
					bSequential = false;
			}
		}

		// Determine which is which
		if((aSequential && bSequential && bHighestUSN > aHighestUSN) || aSequential)
		{
			pLog[i].wVbn = blockB;
			pawMapTable[pLog[i].wLbn] = blockA;
			mapBlockUSN = usnA;
			logBlockUSN = usnB;
			mapBlockLPN = lpnA;
			logBlockLPN = lpnB;
			if(bHighestUSN > highest_usn)
				highest_usn = bHighestUSN;
		} else if((aSequential && bSequential && bHighestUSN <= aHighestUSN) || bSequential)
		{
			pLog[i].wVbn = blockA;
			pawMapTable[pLog[i].wLbn] = blockB;
			mapBlockUSN = usnB;
			logBlockUSN = usnA;
			mapBlockLPN = lpnB;
			logBlockLPN = lpnA;
			if(aHighestUSN > highest_usn)
				highest_usn = aHighestUSN;
		} else
			{
			LOG("ftl: restore failed, we have two non-sequential blocks!\n");
				goto error_release;
			}

		// okay, now we will populate the log block with the correct information.

		for(page = NANDGeometry->pagesPerSuBlk - 1; page >= 0; --page)
		{
			int logOffset;

			// check if empty
			if(logBlockUSN[page] == 0)
				continue;

			// we set it to after the first non-empty page
			if(pLog[i].pagesUsed == 0)
				pLog[i].pagesUsed = page + 1;

			logOffset = logBlockLPN[page] % NANDGeometry->pagesPerSuBlk;

			// is there a newer copy of this page already in this log block?
			if(pLog[i].wPageOffsets[logOffset] != 0xFFFF)
				continue;

			// if not, we'll use it since it's the most recent.
			pLog[i].wPageOffsets[logOffset] = page;
			++pLog[i].pagesCurrent;

			if(logOffset != page)
				pLog[i].isSequential = 0;
		}

		if(pLog[i].pagesUsed != pLog[i].pagesCurrent)
			pLog[i].isSequential = 0;

		LOG("ftl: restore -- log %d, wLbn = %d, wVbn = %d, pagesUsed = %d, pagesCurrent = %d, isSequential = %d\n",
				i, pLog[i].wLbn, pLog[i].wVbn, pLog[i].pagesUsed, pLog[i].pagesCurrent, pLog[i].isSequential);
	}

	pstFTLCxt->nextblockusn = highest_usn + 1;

	for(i = 0; i < numLogs; ++i)
	{
		pLog[i].usn = pstFTLCxt->nextblockusn - 1;
	}

	LOG("ftl: restore successful!\n");

	kfree(usnA);
	kfree(usnB);
	kfree(lpnA);
	kfree(lpnB);
	kfree(blockMap);
	kfree(nonSequential);
	kfree(isEmpty);

	return true;

error_release:
	kfree(usnA);
	kfree(usnB);
	kfree(lpnA);
	kfree(lpnB);
	kfree(blockMap);
	kfree(nonSequential);
	kfree(isEmpty);

	return false;
}

static bool FTL_GetStruct(FTLStruct type, void** data, int* size) {
	switch(type) {
		case FTLCountsTableSID:
			*data = &FTLCountsTable;
			*size = sizeof(FTLCountsTable);
			return true;
		default:
			return false;
	}
}

static bool VFL_GetStruct(FTLStruct type, void** data, int* size) {
	switch(type) {
		case VFLData1SID:
			*data = &VFLData1;
			*size = sizeof(VFLData1);
			return true;
		case VFLData5SID:
			*data = VFLData5;
			*size = 0xF8;
			return true;
		default:
			return false;
	}
}

static bool sum_data(u8* pageBuffer) {
	void* data;
	int size;
	FTL_GetStruct(FTLCountsTableSID, &data, &size);
	FTL_64bit_sum((u64*)pageBuffer, (u64*)data, size);
	VFL_GetStruct(VFLData1SID, &data, &size);
	FTL_64bit_sum((u64*)(pageBuffer + 0x200), (u64*)data, size);
	VFL_GetStruct(VFLData5SID, &data, &size);
	FTL_64bit_sum((u64*)(pageBuffer + 0x400), (u64*)data, size);
	return true;
}

static bool ftl_open_read_counter_tables(void)
{
	int i;
	int pagesToRead;
	bool success;

	pagesToRead = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) / NANDGeometry->bytesPerPage;
	if((((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) % NANDGeometry->bytesPerPage) != 0)
		pagesToRead++;

	success = false;

	for(i = 0; i < pagesToRead; i++) {
		int toRead;
		if(VFL_Read(pstFTLCxt->pages_for_pawEraseCounterTable[i], PageBuffer, (u8*) FTLSpareBuffer, true) != 0)
		{
			success = false;
			goto release;
		}

		toRead = NANDGeometry->bytesPerPage;
		if(toRead > (((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
			toRead = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(((u8*)pstFTLCxt->pawEraseCounterTable) + (i * NANDGeometry->bytesPerPage), PageBuffer, toRead);
	}

	LOG("ftl: Detected version %x %x\n", FTLCxtBuffer->versionLower, FTLCxtBuffer->versionUpper);
	if(FTLCxtBuffer->versionLower == 0x46560001 && FTLCxtBuffer->versionUpper == 0xB9A9FFFE) {
		pagesToRead = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) / NANDGeometry->bytesPerPage;
		if((((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) % NANDGeometry->bytesPerPage) != 0)
			pagesToRead++;

		success = true;
		for(i = 0; i < pagesToRead; i++) {
			int toRead;
			if(VFL_Read(pstFTLCxt->pages_for_pawReadCounterTable[i], PageBuffer, (u8*) FTLSpareBuffer, true) != 0) {
				success = false;
				break;
			}

			toRead = NANDGeometry->bytesPerPage;
			if(toRead > (((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
				toRead = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
			}

			memcpy(((u8*)pstFTLCxt->pawReadCounterTable) + (i * NANDGeometry->bytesPerPage), PageBuffer, toRead);
		}

		if((pstFTLCxt->hasFTLCountsTable + 1) == 0) {
			int x = pstFTLCxt->page_for_FTLCountsTable / NANDGeometry->pagesPerSuBlk;
			if(x == 0 || x <= NANDGeometry->userSuBlksTotal) {
				if(VFL_Read(pstFTLCxt->page_for_FTLCountsTable, PageBuffer, (u8*) FTLSpareBuffer, true) != 0)
				{
					success = false;
					goto release;
				}

				sum_data(PageBuffer);
			}
		}
	} else {
		LOG("ftl: updating the FTL from seemingly compatible version\n");
		for(i = 0; i < (NANDGeometry->userSuBlksTotal + 23); i++) {
			pstFTLCxt->pawReadCounterTable[i] = 0x1388;
		}

		for(i = 0; i < 5; i++) {
			pstFTLCxt->elements2[i].field_0 = -1;
			pstFTLCxt->elements2[i].field_2 = -1;
		}

		pstFTLCxt->field_3C8 = 0;
		pstFTLCxt->clean = 0;
		FTLCxtBuffer->versionLower = 0x46560000;
		FTLCxtBuffer->versionUpper = 0xB9A9FFFF;

		success = true;
	}

release:
	return success;
}

static int FTL_Open(int* pagesAvailable, int* bytesPerPage) {
	int ret;
	int i;
	int pagesToRead;

	u16* pawMapTable = pstFTLCxt->pawMapTable;
	u16* pawEraseCounterTable = pstFTLCxt->pawEraseCounterTable;
	void* pawReadCounterTable = pstFTLCxt->pawReadCounterTable;
	u16* wPageOffsets = pstFTLCxt->wPageOffsets;
	int ftlCxtFound;
	u32 ftlCtrlBlock;
	u32 minUsnDec;

	void* FTLCtrlBlock;
	if((FTLCtrlBlock = VFL_GetFTLCtrlBlock()) == NULL)
		goto FTL_Open_Error;

	memcpy(pstFTLCxt->FTLCtrlBlock, FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));

	// First thing is to get the latest FTLCtrlBlock from the FTLCtrlBlock. It will have the lowest spare.usnDec
	// Again, since a ring buffer is used, the lowest usnDec FTLCxt will be in the FTLCtrlBlock whose first page
	// has the lowest usnDec
	ftlCtrlBlock = 0xffff;
	minUsnDec = 0xffffffff;
	for(i = 0; i < sizeof(pstFTLCxt->FTLCtrlBlock)/sizeof(u16); i++) {
		// read the first page of the block
		ret = VFL_Read(NANDGeometry->pagesPerSuBlk * pstFTLCxt->FTLCtrlBlock[i], PageBuffer, (u8*) FTLSpareBuffer, true);
		if(ret == -EINVAL) {
			goto FTL_Open_Error;
		}

		// 0x43 is the lowest type of FTL control data. Apparently 0x4F would be the highest type.
		if((FTLSpareBuffer->type1 - 0x43) > 0xC)
			continue;	// this block doesn't have FTL data in it! Try the next one

		if(ret != 0)
			continue;	// this block errored out!

		if(ftlCtrlBlock != 0xffff && FTLSpareBuffer->meta.usnDec >= minUsnDec)
			continue;	// we've seen a newer FTLCxtBlock before

		// this is the latest so far
		minUsnDec = FTLSpareBuffer->meta.usnDec;
		ftlCtrlBlock = pstFTLCxt->FTLCtrlBlock[i];
	}


	if(ftlCtrlBlock == 0xffff) {
		LOG("ftl: Cannot find context!\n");
		goto FTL_Open_Error_Release;
	}

	LOG("ftl: Successfully found FTL context block: %d\n", ftlCtrlBlock);

	// The last readable page in this block ought to be a FTLCxt block! If it's any other ftl control page
	// then the shut down was unclean. FTLCxt ought never be the very first page.
	ftlCxtFound = false;
	for(i = NANDGeometry->pagesPerSuBlk - 1; i > 0; i--) {
		ret = VFL_Read(NANDGeometry->pagesPerSuBlk * ftlCtrlBlock + i, PageBuffer, (u8*) FTLSpareBuffer, true);
		if(ret == 1) {
			continue;
		} else if(ret == 0 && FTLSpareBuffer->type1 == 0x43) { // 43 is FTLCxtBlock
			memcpy(FTLCxtBuffer, PageBuffer, sizeof(FTLCxt));
			ftlCxtFound = true;
			break;
		} else {
			if(ret == 0)
				LOG("ftl: Possible unclean shutdown, last FTL metadata type written was 0x%x\n", FTLSpareBuffer->type1);
			else
				LOG("ftl: Error reading FTL context block.\n");

			ftlCxtFound = false;
			break;
		}
	}

	if(!ftlCxtFound)
		goto FTL_Open_Error_Release;

	LOG("ftl: Successfully read FTL context block. usnDec = 0x%x\n", pstFTLCxt->usnDec);

	// Restore now possibly overwritten (by data from NAND) pointers from backed up copies
	pstFTLCxt->pawMapTable = pawMapTable;
	pstFTLCxt->pawEraseCounterTable = pawEraseCounterTable;
	pstFTLCxt->pawReadCounterTable = pawReadCounterTable;
	pstFTLCxt->wPageOffsets = wPageOffsets;

	for(i = 0; i < 18; i++) {
		pstFTLCxt->pLog[i].wPageOffsets = pstFTLCxt->wPageOffsets + (i * NANDGeometry->pagesPerSuBlk);
	}

	pagesToRead = (NANDGeometry->userSuBlksTotal * sizeof(u16)) / NANDGeometry->bytesPerPage;
	if(((NANDGeometry->userSuBlksTotal * sizeof(u16)) % NANDGeometry->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		int toRead;
		if(VFL_Read(pstFTLCxt->pages_for_pawMapTable[i], PageBuffer, (u8*) FTLSpareBuffer, true) != 0)
			goto FTL_Open_Error_Release;

		toRead = NANDGeometry->bytesPerPage;
		if(toRead > ((NANDGeometry->userSuBlksTotal * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
			toRead = (NANDGeometry->userSuBlksTotal * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(((u8*)pstFTLCxt->pawMapTable) + (i * NANDGeometry->bytesPerPage), PageBuffer, toRead);
	}

	pagesToRead = (NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) / NANDGeometry->bytesPerPage;
	if(((NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) % NANDGeometry->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		int toRead;
		if(VFL_Read(pstFTLCxt->pages_for_wPageOffsets[i], PageBuffer, (u8*) FTLSpareBuffer, true) != 0)
			goto FTL_Open_Error_Release;

		toRead = NANDGeometry->bytesPerPage;
		if(toRead > ((NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) - (i * NANDGeometry->bytesPerPage))) {
			toRead = (NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(((u8*)pstFTLCxt->wPageOffsets) + (i * NANDGeometry->bytesPerPage), PageBuffer, toRead);
	}

	pagesToRead = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) / NANDGeometry->bytesPerPage;
	if((((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) % NANDGeometry->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		int toRead;
		if(VFL_Read(pstFTLCxt->pages_for_pawEraseCounterTable[i], PageBuffer, (u8*) FTLSpareBuffer, true) != 0)
			goto FTL_Open_Error_Release;

		toRead = NANDGeometry->bytesPerPage;
		if(toRead > (((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
			toRead = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(((u8*)pstFTLCxt->pawEraseCounterTable) + (i * NANDGeometry->bytesPerPage), PageBuffer, toRead);
	}

	if(ftl_open_read_counter_tables()) {
		CleanFreeVb = true;
		LOG("ftl: FTL successfully opened!\n");
		*pagesAvailable = NANDGeometry->userPagesTotal;
		*bytesPerPage = NANDGeometry->bytesPerPage;
		return 0;
	}

FTL_Open_Error_Release:

FTL_Open_Error:
	LOG("ftl: FTL_Open cannot load FTLCxt!\n");
	CleanFreeVb = false;
	if(FTL_Restore() != false) {
		*pagesAvailable = NANDGeometry->userPagesTotal;
		*bytesPerPage = NANDGeometry->bytesPerPage;
		return 0;
	} else {
		return -EINVAL;
	}
}

u32 FTL_map_page(FTLCxtLog* pLog, int lbn, int offset) {
	if(pLog && pLog->wPageOffsets[offset] != 0xFFFF) {
		if(((pLog->wVbn * NANDGeometry->pagesPerSuBlk) + pLog->wPageOffsets[offset] + 1) != 0)
		{
			return (pLog->wVbn * NANDGeometry->pagesPerSuBlk) + pLog->wPageOffsets[offset];
		}
	}

	return (pstFTLCxt->pawMapTable[lbn] * NANDGeometry->pagesPerSuBlk) + offset;
}

static inline FTLCxtLog* ftl_get_log(u16 lbn)
{
	int i;
	for(i = 0; i < 17; i++) {
		if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
			continue;

		if(pstFTLCxt->pLog[i].wLbn == lbn) {
			return &pstFTLCxt->pLog[i];
		}
	}
	return NULL;
}

int FTL_Read_private(u32 logicalPageNumber, int totalPagesToRead, u8* pBuf)
{
	int i;
	int hasError = false;
	int lbn;
	int offset;
	int ret;
	int pagesRead;
	int pagesToRead;
	int currentLogicalPageNumber;
	FTLCxtLog* pLog;

	FTLCountsTable.totalPagesRead += totalPagesToRead;
	++FTLCountsTable.totalReads;
	pstFTLCxt->totalReadCount++;

	if(!pBuf)
	{
		return -EINVAL;
	}

	if(totalPagesToRead == 0 || (logicalPageNumber + totalPagesToRead) > NANDGeometry->userPagesTotal)
	{
		LOG("ftl: invalid input parameters\n");
		return -EINVAL;
	}

	lbn = logicalPageNumber / NANDGeometry->pagesPerSuBlk;
	offset = logicalPageNumber - (lbn * NANDGeometry->pagesPerSuBlk);

	pLog = ftl_get_log(lbn);

	ret = 0;
	pagesRead = 0;
	currentLogicalPageNumber = logicalPageNumber;

	while(true) {
		int readSuccessful;
		int loop;

		// Read as much as we can from the first logical block
		pagesToRead = NANDGeometry->pagesPerSuBlk - offset;
		if(pagesToRead >= (totalPagesToRead - pagesRead))
			pagesToRead = totalPagesToRead - pagesRead;

		if(pLog != NULL) {
			// we have a scatter entry for this logical block, so we use it
			for(i = 0; i < pagesToRead; i++) {
				ScatteredVirtualPageNumberBuffer[i] = FTL_map_page(pLog, lbn, offset + i);
				if((ScatteredVirtualPageNumberBuffer[i] / NANDGeometry->pagesPerSuBlk) == pLog->wVbn) {
					// This particular page is mapped within one of the log blocks, so we increment for the log block
					pstFTLCxt->pawReadCounterTable[ScatteredVirtualPageNumberBuffer[i] / NANDGeometry->pagesPerSuBlk]++;
				} else {
					// This particular page is mapped to the main block itself, so we increment for that block
					pstFTLCxt->pawReadCounterTable[pstFTLCxt->pawMapTable[lbn]]++;
				}
			}

			readSuccessful = VFL_ReadScatteredPagesInVb(ScatteredVirtualPageNumberBuffer, pagesToRead, pBuf + (pagesRead * NANDGeometry->bytesPerPage), FTLSpareBuffer);
		} else {
			// VFL_ReadMultiplePagesInVb has a different calling convention and implementation than the equivalent iBoot function.
			// Ours is a bit less optimized, and just calls VFL_Read for each page.
			pstFTLCxt->pawReadCounterTable[pstFTLCxt->pawMapTable[lbn]] += pagesToRead;
			readSuccessful = VFL_ReadMultiplePagesInVb(pstFTLCxt->pawMapTable[lbn], offset, pagesToRead, pBuf + (pagesRead * NANDGeometry->bytesPerPage), FTLSpareBuffer);
		}

		loop = 0;
		if(readSuccessful) {
			// check ECC mark for all pages
			for(i = 0; i < pagesToRead; i++) {
				if(FTLSpareBuffer[i].eccMark == 0xFF)
					continue;

				LOG("ftl: CHECK_FTL_ECC_MARK (0x%x, 0x%x, 0x%x, 0x%x)\n", lbn, offset, i, FTLSpareBuffer[i].eccMark);
				hasError = true;
			}

			pagesRead += pagesToRead;
			currentLogicalPageNumber += pagesToRead;
			offset += pagesToRead;

			if(pagesRead == totalPagesToRead) {
				goto FTL_Read_Done;
			}

			loop = false;
		} else {
			loop = true;
		}

		do {
			if(pagesRead != totalPagesToRead && NANDGeometry->pagesPerSuBlk != offset) {
				// there's some remaining pages we have not read before. handle them individually

				int virtualPage = FTL_map_page(pLog, lbn, offset);
				ret = VFL_Read(virtualPage, pBuf + (NANDGeometry->bytesPerPage * pagesRead), (u8*) FTLSpareBuffer, true);

				if(ret == -EINVAL)
					goto FTL_Read_Error_Release;

				if(ret == -EIO || FTLSpareBuffer->eccMark != 0xFF) {
					// ecc error
					LOG("ftl: ECC error, ECC mark is: %x\n", FTLSpareBuffer->eccMark);
					hasError = true;
					if(pLog) {
						virtualPage = FTL_map_page(pLog, lbn, offset);
						LOG("ftl: lbn 0x%x pLog->wVbn 0x%x pawMapTable 0x%x offset 0x%x vpn 0x%x\n", lbn, pLog->wVbn, pstFTLCxt->pawMapTable[lbn], offset, virtualPage);
					} else {
						virtualPage = FTL_map_page(NULL, lbn, offset);
						LOG("ftl: lbn 0x%x pawMapTable 0x%x offset 0x%x vpn 0x%x\n", lbn, pstFTLCxt->pawMapTable[lbn], offset, virtualPage);
					}
				}

				if(ret == 0) {
					if(FTLSpareBuffer->user.logicalPageNumber != offset) {
						// that's not the page we expected there
						LOG("ftl: error, dwWrittenLpn(0x%x) != dwLpn(0x%x)\n", FTLSpareBuffer->user.logicalPageNumber, offset);
					}
				}

				pagesRead++;
				currentLogicalPageNumber++;
				offset++;
				if(pagesRead == totalPagesToRead) {
					goto FTL_Read_Done;
				}
			}

			if(offset == NANDGeometry->pagesPerSuBlk) {
				// go to the next block

				lbn++;
				if(lbn >= NANDGeometry->userSuBlksTotal)
					goto FTL_Read_Error_Release;

				pLog = ftl_get_log(lbn);

				offset = 0;
				break;
			}
		} while(loop);
	}

FTL_Read_Done:
	if(hasError) {
		LOG("ftl: USER_DATA_ERROR, failed with (0x%x, 0x%x, %p)\n", logicalPageNumber, totalPagesToRead, pBuf);
		return -EIO;
	}

	return 0;

FTL_Read_Error_Release:
	LOG("ftl: _FTLRead error!\n");
	return ret;
}

int FTL_Read(u32 logicalPageNumber, int totalPagesToRead, u8* pBuf)
{
	int ret;
	mutex_lock(&ftl_mutex);
	ret = FTL_Read_private(logicalPageNumber, totalPagesToRead, pBuf);
	mutex_unlock(&ftl_mutex);
	return ret;
}

static bool ftl_commit_cxt(void)
{
	int eraseCounterPages;
	int readCounterPages;
	int mapPages;
	int offsetsPages;

	// TODO: We should use the StoreCxt for this. Not only would we be able to more easily do
	// multiplanar writes, but if any of this fails, we can back out without changes to our
	// working context

	int i;
	int totalPages;
	int pagesToWrite;

	u16 curBlock;

	// We need to precalculate how many pages we'd need to write to determine if we should start a new block.

	eraseCounterPages = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) / NANDGeometry->bytesPerPage;
	if((((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) % NANDGeometry->bytesPerPage) != 0)
		eraseCounterPages++;

	readCounterPages = eraseCounterPages;

	mapPages = (NANDGeometry->userSuBlksTotal * sizeof(u16)) / NANDGeometry->bytesPerPage;
	if(((NANDGeometry->userSuBlksTotal * sizeof(u16)) % NANDGeometry->bytesPerPage) != 0)
		mapPages++;

	offsetsPages = (NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) / NANDGeometry->bytesPerPage;
	if(((NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) % NANDGeometry->bytesPerPage) != 0)
		offsetsPages++;

	totalPages = eraseCounterPages + readCounterPages + mapPages + offsetsPages + 1 /* for the SID */ + 1 /* for FTLCxt */;

	curBlock = pstFTLCxt->FTLCtrlPage / NANDGeometry->pagesPerSuBlk;
	if((pstFTLCxt->FTLCtrlPage + totalPages) >= ((curBlock * NANDGeometry->pagesPerSuBlk) + NANDGeometry->pagesPerSuBlk))
	{
		// looks like we would be overflowing into the next block, force the next ctrl page to be on a fresh
		// block in that case
		pstFTLCxt->FTLCtrlPage = (curBlock * NANDGeometry->pagesPerSuBlk) + NANDGeometry->pagesPerSuBlk - 1;
	}

	pagesToWrite = eraseCounterPages;

	for(i = 0; i < pagesToWrite; i++) {
		int toWrite;
		if(!ftl_next_ctrl_page())
		{
			LOG("ftl: cannot allocate next FTL ctrl page\n");
			goto error_release;
		}

		pstFTLCxt->pages_for_pawEraseCounterTable[i] = pstFTLCxt->FTLCtrlPage;

		toWrite = NANDGeometry->bytesPerPage;
		if(toWrite > (((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
			toWrite = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(PageBuffer, ((u8*)pstFTLCxt->pawEraseCounterTable) + (i * NANDGeometry->bytesPerPage), toWrite);
		memset(PageBuffer + toWrite, 0, NANDGeometry->bytesPerPage - toWrite);

		memset(FTLSpareBuffer, 0xFF, sizeof(SpareData));
		FTLSpareBuffer->meta.usnDec = pstFTLCxt->usnDec;
		FTLSpareBuffer->type1 = 0x46;
		FTLSpareBuffer->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_pawEraseCounterTable[i], PageBuffer, (u8*) FTLSpareBuffer) != 0)
			goto error_release;
	}

	pagesToWrite = readCounterPages;

	for(i = 0; i < pagesToWrite; i++) {
		int toWrite;

		if(!ftl_next_ctrl_page())
		{
			LOG("ftl: cannot allocate next FTL ctrl page\n");
			goto error_release;
		}

		pstFTLCxt->pages_for_pawReadCounterTable[i] = pstFTLCxt->FTLCtrlPage;

		toWrite = NANDGeometry->bytesPerPage;
		if(toWrite > (((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
			toWrite = ((NANDGeometry->userSuBlksTotal + 23) * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(PageBuffer, ((u8*)pstFTLCxt->pawReadCounterTable) + (i * NANDGeometry->bytesPerPage), toWrite);
		memset(PageBuffer + toWrite, 0, NANDGeometry->bytesPerPage - toWrite);

		memset(FTLSpareBuffer, 0xFF, sizeof(SpareData));
		FTLSpareBuffer->meta.usnDec = pstFTLCxt->usnDec;
		FTLSpareBuffer->type1 = 0x49;
		FTLSpareBuffer->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_pawReadCounterTable[i], PageBuffer, (u8*) FTLSpareBuffer) != 0)
			goto error_release;
	}

	pagesToWrite = mapPages;

	for(i = 0; i < pagesToWrite; i++) {
		int toWrite;
		if(!ftl_next_ctrl_page())
		{
			LOG("ftl: cannot allocate next FTL ctrl page\n");
			goto error_release;
		}

		pstFTLCxt->pages_for_pawMapTable[i] = pstFTLCxt->FTLCtrlPage;

		toWrite = NANDGeometry->bytesPerPage;
		if(toWrite > ((NANDGeometry->userSuBlksTotal * sizeof(u16)) - (i * NANDGeometry->bytesPerPage))) {
			toWrite = (NANDGeometry->userSuBlksTotal * sizeof(u16)) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(PageBuffer, ((u8*)pstFTLCxt->pawMapTable) + (i * NANDGeometry->bytesPerPage), toWrite);
		memset(PageBuffer + toWrite, 0, NANDGeometry->bytesPerPage - toWrite);

		memset(FTLSpareBuffer, 0xFF, sizeof(SpareData));
		FTLSpareBuffer->meta.usnDec = pstFTLCxt->usnDec;
		FTLSpareBuffer->type1 = 0x44;
		FTLSpareBuffer->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_pawMapTable[i], PageBuffer, (u8*) FTLSpareBuffer) != 0)
			goto error_release;
	}

	pagesToWrite = offsetsPages;

	for(i = 0; i < pagesToWrite; i++) {
		int toWrite;
		if(!ftl_next_ctrl_page())
		{
			LOG("ftl: cannot allocate next FTL ctrl page\n");
			goto error_release;
		}

		pstFTLCxt->pages_for_wPageOffsets[i] = pstFTLCxt->FTLCtrlPage;

		toWrite = NANDGeometry->bytesPerPage;
		if(toWrite > ((NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) - (i * NANDGeometry->bytesPerPage))) {
			toWrite = (NANDGeometry->pagesPerSuBlk * (17 * sizeof(u16))) - (i * NANDGeometry->bytesPerPage);
		}

		memcpy(PageBuffer, ((u8*)pstFTLCxt->wPageOffsets) + (i * NANDGeometry->bytesPerPage), toWrite);
		memset(PageBuffer + toWrite, 0, NANDGeometry->bytesPerPage - toWrite);

		memset(FTLSpareBuffer, 0xFF, sizeof(SpareData));
		FTLSpareBuffer->meta.usnDec = pstFTLCxt->usnDec;
		FTLSpareBuffer->type1 = 0x45;
		FTLSpareBuffer->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_wPageOffsets[i], PageBuffer, (u8*) FTLSpareBuffer) != 0)
			goto error_release;
	}

	{
		u32 unkSID;
		void* data;
		int size;


		if(!ftl_next_ctrl_page())
		{
			LOG("ftl: cannot allocate next FTL ctrl page\n");
			goto error_release;
		}

		pstFTLCxt->page_for_FTLCountsTable = pstFTLCxt->FTLCtrlPage;
		pstFTLCxt->hasFTLCountsTable = 0xFFFFFFFF;

		memset(PageBuffer, 0, NANDGeometry->bytesPerPage);

		FTL_GetStruct(FTLCountsTableSID, &data, &size);
		memcpy(PageBuffer, data, size);
		VFL_GetStruct(VFLData1SID, &data, &size);
		memcpy(PageBuffer + 0x200, data, size);
		VFL_GetStruct(VFLData5SID, &data, &size);
		memcpy(PageBuffer + 0x400, data, size);

		unkSID = 0x10001;
		memcpy(PageBuffer + NANDGeometry->bytesPerPage - sizeof(unkSID), &unkSID, sizeof(unkSID));

		memset(FTLSpareBuffer, 0xFF, sizeof(SpareData));
		FTLSpareBuffer->meta.usnDec = pstFTLCxt->usnDec;
		FTLSpareBuffer->type1 = 0x47;
		FTLSpareBuffer->meta.idx = 0;

		if(VFL_Write(pstFTLCxt->page_for_FTLCountsTable, PageBuffer, (u8*) FTLSpareBuffer) != 0)
			goto error_release;
	}

	if(!ftl_next_ctrl_page())
	{
		LOG("ftl: cannot allocate next FTL ctrl page\n");
		goto error_release;
	}

	pstFTLCxt->clean = 1;

	memset(FTLSpareBuffer, 0xFF, sizeof(SpareData));
	FTLSpareBuffer->meta.usnDec = pstFTLCxt->usnDec;
	FTLSpareBuffer->type1 = 0x43;
	if(VFL_Write(pstFTLCxt->FTLCtrlPage, (u8*) pstFTLCxt, (u8*) FTLSpareBuffer) != 0)
		goto error_release;

	return true;

error_release:
	LOG("ftl: error committing FTLCxt!\n");

	return false;
}

void check_for_dirty_free_vb(u8* pageBuffer, SpareData* spareData)
{
	int i;
	int curFreeIdx = pstFTLCxt->nextFreeIdx;

	if(CleanFreeVb)
		return;

	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; ++i)
	{
		if(pstFTLCxt->awFreeVb[curFreeIdx] != 0xFFFF)
		{
			int page;
			int block = pstFTLCxt->awFreeVb[curFreeIdx];
			for(page = 0; page < NANDGeometry->pagesPerSuBlk; ++page)
			{
				int ret = VFL_Read(block * NANDGeometry->pagesPerSuBlk + page, pageBuffer, (u8*) spareData, true);

				if(ret == ERROR_EMPTYBLOCK)
					continue;

				LOG("ftl: free block %d has non-empty pages.\r\n", block);
				VFL_Erase(block);
				break;
			}
		}
		curFreeIdx = (curFreeIdx + 1) % 20;
	}

	CleanFreeVb = true;
}

static bool ftl_mark_unclean(void)
{
	int i;
	u8* pageBuffer;
	u8* spareBuffer;

	if(!pstFTLCxt->clean)
		return true;

	pageBuffer = (u8*) kmalloc(NANDGeometry->bytesPerPage, GFP_KERNEL | GFP_DMA);
	spareBuffer = (u8*) kmalloc(NANDGeometry->bytesPerSpare, GFP_KERNEL | GFP_DMA);
	if(!pageBuffer || !spareBuffer)
	{
		LOG("ftl: ftl_mark_unclean: out of memory\n");
		return false;
	}

	check_for_dirty_free_vb(pageBuffer, (SpareData*) spareBuffer);

	for(i = 0; i < 3; ++i)
	{
		u16 block;

		if(!ftl_next_ctrl_page())
		{
			LOG("ftl: ftl_mark_unclean: could not get a ctrl page\n");
			goto error_release;
		}

		memset(pageBuffer, 0xFF, NANDGeometry->bytesPerPage);
		((SpareData*)spareBuffer)->type1 = 0x4F;

		if(VFL_Write(pstFTLCxt->FTLCtrlPage, pageBuffer, spareBuffer) == 0)
		{
			pstFTLCxt->clean = 0;
			kfree(pageBuffer);
			kfree(spareBuffer);
			return true;
		}

		// have an error, try again on the next block.
		block = pstFTLCxt->FTLCtrlPage / NANDGeometry->pagesPerSuBlk;
		pstFTLCxt->FTLCtrlPage = (block * NANDGeometry->pagesPerSuBlk) + NANDGeometry->pagesPerSuBlk - 1;
	}

	LOG("ftl: ftl_mark_unclean failed!\n");

error_release:
	kfree(pageBuffer);
	kfree(spareBuffer);
	return false;
}

static FTLCxtLog* ftl_prepare_log(u16 lbn)
{
	FTLCxtLog* pLog = ftl_get_log(lbn);

	if(pLog == NULL)
	{
		int i;
		for(i = 0; i < 17; ++i)
		{
			if((pstFTLCxt->pLog[i].wVbn != 0xFFFF) && (pstFTLCxt->pLog[i].pagesUsed == 0))
			{
				pLog = &pstFTLCxt->pLog[i];
				break;
			}
		}

		if(pLog == NULL)
		{
			if(pstFTLCxt->wNumOfFreeVb < 3)
			{
				LOG("ftl: prepare_log WARNING: Pool block leak detected!\n");
				return NULL;
			} else if(pstFTLCxt->wNumOfFreeVb == 3)
			{
				if(!ftl_merge(NULL))
				{
					LOG("ftl: block merged failed!\n");
					return NULL;
				}
			}

			for(i = 0; i < 17; ++i)
			{
				if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
				       break;
			}

			pLog = &pstFTLCxt->pLog[i];

			if(!ftl_get_free_vb(&pLog->wVbn))
			{
				LOG("ftl: prepare_log could not get free vb\n");
				return NULL;
			}
		}

		memset(pLog->wPageOffsets, 0xFF, NANDGeometry->pagesPerSuBlk * sizeof(u16));
		pLog->wLbn = lbn;
		pLog->pagesUsed = 0;
		pLog->pagesCurrent = 0;
		pLog->isSequential = 1;

#ifdef IPHONE_DEBUG
		LOG("ftl: new log for lbn %d is %d\n", pLog->wLbn, pLog->wVbn);
#endif
	}

	pLog->usn = pstFTLCxt->nextblockusn - 1;

	return pLog;
}

static inline void ftl_check_still_sequential(FTLCxtLog* pLog, u32 page)
{
	if((pLog->pagesUsed != pLog->pagesCurrent) || (pLog->wPageOffsets[page] != page))
		pLog->isSequential = 0;
}

static bool ftl_copy_page(u32 src, u32 dest, u32 lpn, u32 isSequential)
{
	u8* pageBuffer = kmalloc(NANDGeometry->bytesPerPage, GFP_KERNEL | GFP_DMA);
	SpareData* spareData = (SpareData*) kmalloc(NANDGeometry->bytesPerSpare, GFP_KERNEL | GFP_DMA);

	int ret = VFL_Read(src, pageBuffer, (u8*) spareData, true);

	memset(spareData, 0xFF, NANDGeometry->bytesPerSpare);

	if(ret == ERROR_EMPTYBLOCK)
		memset(pageBuffer, 0, NANDGeometry->bytesPerPage);
	else if(ret != 0)
		spareData->eccMark = 0x55;

	// FIXME: In CPICH, not all uses of this seem to increment this value. Find out whose fail it is.
	// Incrementing shouldn't hurt, though.
	spareData->user.usn = ++pstFTLCxt->nextblockusn;
	spareData->user.logicalPageNumber = lpn;

	// This isn't always done either
	if(isSequential == 1 && ((dest % NANDGeometry->pagesPerSuBlk) == (NANDGeometry->pagesPerSuBlk - 1)))
		spareData->type1 = 0x41;
	else
		spareData->type1 = 0x40;

	ret = VFL_Write(dest, pageBuffer, (u8*) spareData);
	if(ret != 0)
		goto error_release;

	kfree(pageBuffer);
	kfree(spareData);
	return true;

error_release:
	kfree(pageBuffer);
	kfree(spareData);

	return false;
}

static bool ftl_copy_block(u16 lSrc, u16 vDest)
{
	int i;
	int error = false;
	u8* pageBuffer = kmalloc(NANDGeometry->bytesPerPage, GFP_KERNEL | GFP_DMA);
	SpareData* spareData = (SpareData*) kmalloc(NANDGeometry->bytesPerSpare, GFP_KERNEL | GFP_DMA);

	++pstFTLCxt->nextblockusn;

	for(i = 0; i < NANDGeometry->pagesPerSuBlk; ++i)
	{
		int ret = FTL_Read_private(lSrc * NANDGeometry->pagesPerSuBlk + i, 1, pageBuffer);
		memset(spareData, 0xFF, NANDGeometry->bytesPerSpare);
		if(ret)
			spareData->eccMark = 0x55;

		spareData->user.logicalPageNumber = lSrc * NANDGeometry->pagesPerSuBlk + i;
		spareData->user.usn = pstFTLCxt->nextblockusn;
		if(i == (NANDGeometry->pagesPerSuBlk - 1))
			spareData->type1 = 0x41;
		else
			spareData->type1 = 0x40;

		if(VFL_Write(vDest * NANDGeometry->pagesPerSuBlk + i, pageBuffer, (u8*) spareData) != 0)
		{
			error = true;
			break;
		}
	}

	if(error)
	{
		++pstFTLCxt->pawEraseCounterTable[vDest];
		pstFTLCxt->pawReadCounterTable[vDest] = 0;

		if(VFL_Erase(vDest) != 0)
		{
			LOG("ftl: ftl_copy_block failed to erase after failure!\n");
			goto error_release;
		}

		LOG("ftl: ftl_copy_block failed!\n");
		goto error_release;
	}

	kfree(pageBuffer);
	kfree(spareData);
	return true;

error_release:
	kfree(pageBuffer);
	kfree(spareData);

	return false;
}

static bool ftl_compact_scattered(FTLCxtLog* pLog)
{
	int error;
	int i;

	LOG("ftl: ftl_compact_scattered\n");

	++FTLCountsTable.compactScatteredCount;

	if(pLog->pagesCurrent == 0)
	{
		// nothing useful in here, just release it.
		if(!ftl_set_free_vb(pLog->wVbn))
		{
			LOG("ftl: ftl_compact_scattered cannot release vb!\n");
		}

		pLog->wVbn = 0xFFFF;
		++pstFTLCxt->swapCounter;
		return true;
	}

	// make a backup
	pstFTLCxt->pLog[17].usn  = pLog->usn;
	pstFTLCxt->pLog[17].wVbn  = pLog->wVbn;
	pstFTLCxt->pLog[17].wLbn  = pLog->wLbn;
	pstFTLCxt->pLog[17].pagesUsed  = pLog->pagesUsed;
	pstFTLCxt->pLog[17].pagesCurrent  = pLog->pagesCurrent;
	pstFTLCxt->pLog[17].isSequential  = pLog->isSequential;

	memcpy(pstFTLCxt->pLog[17].wPageOffsets, pLog->wPageOffsets, NANDGeometry->pagesPerSuBlk);

	error = false;

	for(i = 0; i < 4; ++i)
	{
		int page;
		u16 newBlock;
		if(!ftl_get_free_vb(&newBlock))
		{
			LOG("ftl: ftl_compact_scattered ran out of free vb!\n");
			goto error_release;
		}

		pLog->pagesUsed = 0;
		pLog->pagesCurrent = 0;
		pLog->isSequential = 1;
		pLog->wVbn = newBlock;

		for(page = 0; page < NANDGeometry->pagesPerSuBlk; ++page)
		{
			if(pLog->wPageOffsets[page] != 0xFFFF)
			{
				u32 lpn = pLog->wLbn * NANDGeometry->pagesPerSuBlk + page;
				u32 newPage = newBlock * NANDGeometry->pagesPerSuBlk + pLog->pagesUsed;
				u32 oldPage = pstFTLCxt->pLog[17].wVbn * NANDGeometry->pagesPerSuBlk + pLog->wPageOffsets[page];
				if(!ftl_copy_page(oldPage, newPage, lpn, pLog->isSequential))
				{
					error = true;
					break;
				}
				pLog->wPageOffsets[page] = pLog->pagesUsed++;
				++pLog->pagesCurrent;
				ftl_check_still_sequential(pLog, page);
			}
		}

		if(pstFTLCxt->pLog[17].pagesCurrent != pLog->pagesCurrent)
			error = true;

		if(!error)
		{
			if(!ftl_set_free_vb(pstFTLCxt->pLog[17].wVbn))
			{
				LOG("ftl: ftl_compact_scattered could not set old Vb free!\n");
				goto error_release;
			}

			return true;
		}

		if(!ftl_set_free_vb(pLog->wVbn))
		{
			LOG("ftl: ftl_compact_scattered could not set failed Vb free!\n");
			// better just to continue anyway
		}

		// restore the backup
		pLog->usn = pstFTLCxt->pLog[17].usn;
		pLog->wVbn = pstFTLCxt->pLog[17].wVbn;
		pLog->wLbn = pstFTLCxt->pLog[17].wLbn;
		pLog->pagesUsed = pstFTLCxt->pLog[17].pagesUsed;
		pLog->pagesCurrent = pstFTLCxt->pLog[17].pagesCurrent;
		pLog->isSequential = pstFTLCxt->pLog[17].isSequential;

		memcpy(pLog->wPageOffsets, pstFTLCxt->pLog[17].wPageOffsets, NANDGeometry->pagesPerSuBlk);
	}

error_release:
	return false;
}

static bool ftl_simple_merge(FTLCxtLog* pLog)
{
	int i;
	int error = false;
	u16 block;

	LOG("ftl: ftl_simple_merge\n");

	++FTLCountsTable.simpleMergeCount;

	for(i = 0; i < 4; ++i)
	{
		if(!ftl_get_free_vb(&block))
		{
			LOG("ftl: ftl_simple_merge can't get free vb!\n");
			return false;
		}

		if(ftl_copy_block(pLog->wLbn, block))
		{
			error = false;
			break;
		}

		error = true;
		if(!ftl_set_free_vb(block))
		{
			LOG("ftl: ftl_simple_merge can't set free vb after failure!\n");
			return false;
		}
	}

	if(error)
	{
		LOG("ftl: ftl_simple_merge failed!\n");
		return false;
	}

	if(!ftl_set_free_vb(pLog->wVbn))
	{
			LOG("ftl: ftl_simple_merge can't set free scatter vb!\n");
			return false;
	}

	pLog->wVbn = 0xFFFF;

	if(!ftl_set_free_vb(pstFTLCxt->pawMapTable[pLog->wLbn]))
	{
			LOG("ftl: ftl_simple_merge can't set free map vb!\n");
			return false;
	}

	pstFTLCxt->pawMapTable[pLog->wLbn] = block;

	return true;
}

static bool ftl_copy_merge(FTLCxtLog* pLog)
{
	LOG("ftl: ftl_copy_merge\n");

	if((pLog->isSequential != 1) || (pLog->pagesCurrent != pLog->pagesUsed))
	{
		LOG("ftl: attempted ftl_copy_merge on non-sequential scatter block!\n");
		return false;
	}

	if(pLog->pagesUsed >= NANDGeometry->pagesPerSuBlk)
		++FTLCountsTable.copyMergeWhileFullCount;
	else
		++FTLCountsTable.copyMergeWhileNotFullCount;

	for(; pLog->pagesUsed < NANDGeometry->pagesPerSuBlk; ++pLog->pagesUsed)
	{
		u32 lpn = pLog->wLbn * NANDGeometry->pagesPerSuBlk  + pLog->pagesUsed;
		u32 newPage = pLog->wVbn * NANDGeometry->pagesPerSuBlk + pLog->pagesUsed;
		u32 oldPage = pstFTLCxt->pawMapTable[pLog->wLbn] * NANDGeometry->pagesPerSuBlk + pLog->pagesUsed;

		// wtf, this isn't really all sequential!
		if(pLog->wPageOffsets[pLog->pagesUsed] != 0xFFFF)
			return ftl_simple_merge(pLog);

		// try the simple merge if this fails
		if(!ftl_copy_page(oldPage, newPage, lpn, 1))
			return ftl_simple_merge(pLog);
	}

	if(!ftl_set_free_vb(pstFTLCxt->pawMapTable[pLog->wLbn]))
	{
			LOG("ftl: ftl_copy_merge can't set free map vb!\n");
			return false;
	}

	// replace it with our now completed block
	pstFTLCxt->pawMapTable[pLog->wLbn] = pLog->wVbn;

	// set this log block as free
	pLog->wVbn = 0xFFFF;

	return true;
}

static bool ftl_merge(FTLCxtLog* pLog)
{
	u32 oldest = 0xFFFFFFFF;
	u32 mostCurrent = 0;

	if(!ftl_mark_unclean())
	{
		LOG("ftl: merge failed - cannot open new mark context\n");
		return false;
	}

	if(pLog == NULL)
	{
		int i;

		// find one to swap out
		for(i = 0; i < 17; ++i)
		{
			if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
				continue;

			if(pstFTLCxt->pLog[i].pagesUsed == 0 || pstFTLCxt->pLog[i].pagesCurrent == 0)
			{
				LOG("ft: merge error - we still have logs that can be used instead!\n");
				return false;
			}

			if(pstFTLCxt->pLog[i].usn < oldest || (pstFTLCxt->pLog[i].usn == oldest && pstFTLCxt->pLog[i].pagesCurrent > mostCurrent))
			{
				pLog = &pstFTLCxt->pLog[i];
				oldest = pstFTLCxt->pLog[i].usn;
				mostCurrent = pstFTLCxt->pLog[i].pagesCurrent;
			}
		}

		if(pLog == NULL)
			return false;
	} else if(pLog->pagesCurrent < (NANDGeometry->pagesPerSuBlk / 2))
	{
		// less than half the pages in this log seems to be current, let's get rid of the crap and just reuse this one.

		++pstFTLCxt->swapCounter;
		return ftl_compact_scattered(pLog);
	}

	if(pLog->isSequential == 1)
	{
		if(!ftl_copy_merge(pLog))
		{
			LOG("ftl: simple merge failed\n");
			return false;
		}
		++pstFTLCxt->swapCounter;
		return true;
	}
	else
	{
		if(!ftl_simple_merge(pLog))
		{
			LOG("ftl: simple merge failed\n");
			return false;
		}
		++pstFTLCxt->swapCounter;
		return true;
	}
}

bool ftl_auto_wearlevel(void)
{
	int i;
	u16 smallestEraseCount = 0xFFFF;
	u16 leastErasedBlock = 0;
	u16 leastErasedBlockLbn = 0;
	u16 largestEraseCount = 0;
	u16 mostErasedFreeBlock = 0;
	u16 mostErasedFreeBlockIdx = 20;

	LOG("ftl: ftl_auto_wearlevel\n");

	if(!ftl_mark_unclean())
	{
		LOG("ftl: auto wearlevel failed due to mark_unclean failure\n");
		return false;
	}

	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; ++i)
	{
		int idx = (pstFTLCxt->nextFreeIdx + i) % 20;

		if(pstFTLCxt->awFreeVb[idx] == 0xFFFF)
			continue;

		if((mostErasedFreeBlockIdx == 20) || pstFTLCxt->pawEraseCounterTable[pstFTLCxt->awFreeVb[idx]] > largestEraseCount)
		{
			mostErasedFreeBlockIdx = idx;
			mostErasedFreeBlock = pstFTLCxt->awFreeVb[idx];
			largestEraseCount = pstFTLCxt->pawEraseCounterTable[mostErasedFreeBlock];
		}
	}

	for(i = 0; i < NANDGeometry->userSuBlksTotal; ++i)
	{
		if(pstFTLCxt->pawEraseCounterTable[pstFTLCxt->pawMapTable[i]] > largestEraseCount)
			largestEraseCount = pstFTLCxt->pawEraseCounterTable[pstFTLCxt->pawMapTable[i]];

		// don't swap stuff with log blocks attached
		if(ftl_get_log(i) != NULL)
			continue;

		if(pstFTLCxt->pawEraseCounterTable[pstFTLCxt->pawMapTable[i]] < smallestEraseCount)
		{
			leastErasedBlockLbn = i;
			leastErasedBlock = pstFTLCxt->pawMapTable[i];
			smallestEraseCount = pstFTLCxt->pawEraseCounterTable[leastErasedBlock];
		}
	}

	if(largestEraseCount == 0)
		return true;

	if((largestEraseCount - smallestEraseCount) < 5)
		return true;

	++pstFTLCxt->pawEraseCounterTable[mostErasedFreeBlock];
	pstFTLCxt->pawReadCounterTable[mostErasedFreeBlock] = 0;

	if(VFL_Erase(mostErasedFreeBlock) != 0)
	{
		LOG("ftl: auto wear-level cannot erase most erased free block\n");
		return false;
	}

	++FTLCountsTable.blockSwapCount;

	if(!ftl_copy_block(leastErasedBlockLbn, mostErasedFreeBlock))
	{
		LOG("ftl: auto wear-level cannot copy least erased to most erased\n");
		return false;
	}

	pstFTLCxt->pawMapTable[leastErasedBlockLbn] = mostErasedFreeBlock;

	++pstFTLCxt->pawEraseCounterTable[leastErasedBlock];
	pstFTLCxt->pawReadCounterTable[leastErasedBlock] = 0;

	if(VFL_Erase(leastErasedBlock) != 0)
	{
		LOG("ftl: auto wear-level cannot erase previously least erased block\n");
		return false;
	}

	pstFTLCxt->awFreeVb[mostErasedFreeBlockIdx] = leastErasedBlock;

	return true;
}

static int FTL_Write_private(u32 logicalPageNumber, int totalPagesToWrite, u8* pBuf)
{
	int i;

	//LOG("FTL_Write start\n");
#ifdef IPHONE_DEBUG
	LOG("request to write %d pages at %d\r\n", totalPagesToWrite, logicalPageNumber);
#endif

	FTLCountsTable.totalPagesWritten += totalPagesToWrite;
	++FTLCountsTable.totalWrites;

	if(!pBuf) {
		return -EINVAL;
	}

	if(totalPagesToWrite == 0 || (logicalPageNumber + totalPagesToWrite) > NANDGeometry->userPagesTotal) {
		LOG("ftl: write has invalid input parameters\n");
		return -EINVAL;
	}

	if(!ftl_mark_unclean())
	{
		LOG("ftl: write could not mark FTL as unclean!\n");
		return -EINVAL;
	}

	for(i = 0; i < totalPagesToWrite; )
	{
		int lbn = (logicalPageNumber + i) / NANDGeometry->pagesPerSuBlk;
		int offset = (logicalPageNumber + i) - (lbn * NANDGeometry->pagesPerSuBlk);

		FTLCxtLog* pLog = ftl_prepare_log(lbn);

		if(pLog == NULL)
		{
			LOG("ftl: write failed to prepare log!\n");
			goto error_release;
		}

		if(offset == 0 && (totalPagesToWrite - i) >= NANDGeometry->pagesPerSuBlk)
		{
			// we are replacing an entire block

			u16 vblock;
			int j;

			if(pLog->pagesUsed != 0)
			{
				// we can't use this log block since it's not empty, get rid of it.
				if(!ftl_set_free_vb(pLog->wVbn))
				{
					LOG("ftl: write failed to set a free Vb when replacing an entire block!\n");
					goto error_release;
				}

				if(!ftl_get_free_vb(&vblock))
				{
					LOG("ftl: write failed to get a free Vb for replacing an entire block!\n");
					goto error_release;
				}
			} else
			{
				// we can just use this empty log block
				vblock = pLog->wVbn;
			}

			// don't need this log block anymore, we're now a full map block
			pLog->wVbn = 0xFFFF;

			++pstFTLCxt->nextblockusn;

			for(j = 0; j < NANDGeometry->pagesPerSuBlk; ++j)
			{
				int tries;

				memset(FTLSpareBuffer, 0xFF, NANDGeometry->bytesPerSpare);
				FTLSpareBuffer->user.logicalPageNumber = logicalPageNumber + i + j;
				FTLSpareBuffer->user.usn = pstFTLCxt->nextblockusn;
				if(j == (NANDGeometry->pagesPerSuBlk - 1))
					FTLSpareBuffer->type1 = 0x41;
				else
					FTLSpareBuffer->type1 = 0x40;

				for(tries = 0; tries < 4; ++tries)
				{
					if(VFL_Write((vblock * NANDGeometry->pagesPerSuBlk) + j,
							pBuf + ((i + j) * (NANDGeometry->bytesPerPage)), (u8*) FTLSpareBuffer) == 0)
						break;
				}

				if(tries == 4)
				{
					LOG("ftl: write error during writing replacement block!\n");
					// FIXME: no real error handling here!
				}
			}

			if(!ftl_set_free_vb(pstFTLCxt->pawMapTable[lbn]))
			{
				LOG("ftl: write failed to set a free Vb after writing replacement block!\n");
				goto error_release;
			}

			pstFTLCxt->pawMapTable[lbn] = vblock;
#ifdef IPHONE_DEBUG
			LOG("ftl: replacing block with stuff from offset %d\n", (i * (NANDGeometry->bytesPerPage)));
#endif
			i += NANDGeometry->pagesPerSuBlk;
		} else
		{
			int pagesCanWrite;
			int j;

			// we'll have to use the log since we're not replacing the whole block

			if(pLog->pagesUsed == NANDGeometry->pagesPerSuBlk)
			{
#ifdef IPHONE_DEBUG
				int orig = pLog->wVbn;
#endif

				// oh no, this log is full. we have to commit it
				if(!ftl_merge(pLog))
				{
					LOG("ftl: write failed to merge in the log!\n");
					goto error_release;
				}

				pLog = ftl_prepare_log(lbn);

				if(pLog == NULL)
				{
					LOG("ftl: write failed to prepare log after merging!\n");
					goto error_release;
				}
#ifdef IPHONE_DEBUG
				LOG("ftl: replaced pLog for wLbn %d wVbn %d => %d\n", pLog->wLbn, orig, pLog->wVbn);
#endif
			}

			pagesCanWrite = totalPagesToWrite - i;

			// don't overflow the log
			if(pagesCanWrite > (NANDGeometry->pagesPerSuBlk - pLog->pagesUsed))
				pagesCanWrite = NANDGeometry->pagesPerSuBlk - pLog->pagesUsed;

			// don't write pages belonging to some other block here
			if(pagesCanWrite > (NANDGeometry->pagesPerSuBlk - offset))
				pagesCanWrite = NANDGeometry->pagesPerSuBlk - offset;

			for(j = 0; j < pagesCanWrite; ++j)
			{
				int tries;
				int abspage;

				memset(FTLSpareBuffer, 0xFF, NANDGeometry->bytesPerSpare);
				FTLSpareBuffer->user.logicalPageNumber = logicalPageNumber + i + j;
				FTLSpareBuffer->user.usn = ++pstFTLCxt->nextblockusn;
				if((pLog->pagesUsed == (NANDGeometry->pagesPerSuBlk - 1)) && pLog->isSequential)
					FTLSpareBuffer->type1 = 0x41;
				else
					FTLSpareBuffer->type1 = 0x40;

				for(tries = 0; tries < 4; ++tries)
				{
					abspage = pLog->wVbn * NANDGeometry->pagesPerSuBlk + pLog->pagesUsed;

					if(VFL_Write(abspage,
							pBuf + ((i + j) * (NANDGeometry->bytesPerPage)), (u8*) FTLSpareBuffer) == 0)
						break;
					++pLog->pagesUsed;
				}

				if(tries == 4)
				{
					LOG("ftl: write error during writing to log!\n");
					// FIXME: no real error handling here
				}

				if(pLog->wPageOffsets[offset + j] == 0xFFFF)
				{
					// This page wasn't current before but now it is
					++pLog->pagesCurrent;
				}

				pLog->wPageOffsets[offset + j] = pLog->pagesUsed;
				++pLog->pagesUsed;

				if(pLog->isSequential == 1)
					ftl_check_still_sequential(pLog, offset + j);

#ifdef IPHONE_DEBUG
				LOG("ftl: filling out block %d log entry %d = %d logical (%d, %d) with stuff from offset %d\n", pLog->wVbn,
						pLog->pagesUsed - 1,
						abspage, pLog->wLbn, offset + j, ((i + j) * (NANDGeometry->bytesPerPage)));
#endif

			}

			i += pagesCanWrite;
		}

	}

	if(pstFTLCxt->swapCounter >= 300)
	{
		int tries;
		for(tries = 0; tries < 4; ++tries)
		{
			if(ftl_auto_wearlevel())
			{
				pstFTLCxt->swapCounter -= 20;
				break;
			}
		}
	}

	//LOG("FTL_Write end\n");
	return 0;

error_release:
	//LOG("FTL_Write end\n");
	return -EINVAL;
}

int FTL_Write(u32 logicalPageNumber, int totalPagesToWrite, u8* pBuf)
{
	int ret;

#ifdef FTL_PROFILE
	u64 startTime;
#endif

	mutex_lock(&ftl_mutex);

#ifdef FTL_PROFILE
	Time_wait_for_ecc_interrupt = 0;
	Time_wait_for_ready = 0;
	Time_wait_for_address_done = 0;
	Time_wait_for_command_done = 0;
	Time_wait_for_transfer_done = 0;
	Time_wait_for_nand_bank_ready = 0;
	Time_nand_write = 0;
	Time_iphone_dma_finish = 0;
	TotalWriteTime = 0;
	startTime = iphone_microtime();
#endif

	ret = FTL_Write_private(logicalPageNumber, totalPagesToWrite, pBuf);

#ifdef FTL_PROFILE
	TotalWriteTime += iphone_microtime() - startTime;
	LOG("write complete in %llu: ecc = %llu, ready = %llu, addr = %llu, cmd = %llu, xfer = %llu, bankrdy = %llu, dma = %llu, write = %llu, active = %llu\n",
			TotalWriteTime,
			Time_wait_for_ecc_interrupt,
			Time_wait_for_ready,
			Time_wait_for_address_done,
			Time_wait_for_command_done,
			Time_wait_for_transfer_done,
			Time_wait_for_nand_bank_ready,
			Time_iphone_dma_finish,
			Time_nand_write,
			TotalWriteTime - Time_wait_for_ecc_interrupt - Time_wait_for_ready - Time_wait_for_address_done - Time_wait_for_command_done - Time_wait_for_transfer_done - Time_wait_for_nand_bank_ready - Time_iphone_dma_finish
			);
#endif

	mutex_unlock(&ftl_mutex);
	return ret;
}

bool ftl_sync(void)
{
	int tries;

#ifdef FTL_PROFILE
	u64 startTime;
#endif

	mutex_lock(&ftl_mutex);
	//LOG("ftl_sync start\n");

#ifdef FTL_PROFILE
	startTime = iphone_microtime();
#endif

	if(pstFTLCxt->clean)
	{
		LOG("ftl_sync end\n");
		mutex_unlock(&ftl_mutex);
		return true;
	}

	check_for_dirty_free_vb(PageBuffer, FTLSpareBuffer);

	if(pstFTLCxt->swapCounter >= 20)
	{
		for(tries = 0; tries < 4; ++ tries)
		{
			if(ftl_auto_wearlevel())
			{
				pstFTLCxt->swapCounter -= 20;
				break;
			}
		}
	}

	for(tries = 0; tries < 4; ++ tries)
	{
		if(ftl_commit_cxt())
		{
			//LOG("ftl_sync end\n");

#ifdef FTL_PROFILE
			TotalSyncTime += iphone_microtime() - startTime;
#endif
			mutex_unlock(&ftl_mutex);
			return true;
		} else
		{
			// have some kind of error, try again on a new block
			u16 block = pstFTLCxt->FTLCtrlPage / NANDGeometry->pagesPerSuBlk;
			pstFTLCxt->FTLCtrlPage = (block * NANDGeometry->pagesPerSuBlk) + NANDGeometry->pagesPerSuBlk - 1;
		}
	}

	//LOG("ftl_sync end\n");

#ifdef FTL_PROFILE
	TotalSyncTime += iphone_microtime() - startTime;
#endif

	mutex_unlock(&ftl_mutex);
	return false;
}

int ftl_setup(void)
{
	int pagesAvailable;
	int bytesPerPage;

	mutex_lock(&ftl_mutex);

	if(VFL_Init() != 0)
	{
		LOG("ftl: VFL_Init failed\n");
		mutex_unlock(&ftl_mutex);
		return -1;
	}

	if(VFL_Verify() != 0)
	{
		LOG("ftl: VFL_Verify failed\n");
		mutex_unlock(&ftl_mutex);
		return -1;
	}

	if(VFL_Open() != 0) {
		LOG("ftl: VFL_Open failed\n");
		mutex_unlock(&ftl_mutex);
		return -1;
	}

	if(FTL_Init() != 0) {
		LOG("ftl: FTL_Init failed\n");
		mutex_unlock(&ftl_mutex);
		return -1;
	}

	if(FTL_Open(&pagesAvailable, &bytesPerPage) != 0) {
		LOG("ftl: FTL_Open failed\n");
		mutex_unlock(&ftl_mutex);
		return -1;
	}

	mutex_unlock(&ftl_mutex);

	return 0;
}
