#ifndef IPHONE_VFL_H
#define IPHONE_VFL_H

#include <linux/io.h>
#include <ftl/nand.h>

typedef struct VFLData1Type {
	u64 field_0;
	u64 field_8;
	u64 field_10;
	u64 field_18;
	u64 field_20;
	u64 field_28;
	u64 field_30;
	u64 field_38;
	u64 field_40;
} VFLData1Type;

int VFL_Verify(void);
int VFL_Init(void);
int VFL_Open(void);
int VFL_StoreFTLCtrlBlock(u16* ftlctrlblock);
u16* VFL_GetFTLCtrlBlock(void);
bool VFL_ReadScatteredPagesInVb(u32* virtualPageNumber, int count, u8* main, SpareData* spare);
bool VFL_ReadMultiplePagesInVb(int logicalBlock, int logicalPage, int count, u8* main, SpareData* spare);
int VFL_Write(u32 virtualPageNumber, u8* buffer, u8* spare);
int VFL_Read(u32 virtualPageNumber, u8* buffer, u8* spare, bool empty_ok);
int VFL_Erase(u16 block);

#endif
