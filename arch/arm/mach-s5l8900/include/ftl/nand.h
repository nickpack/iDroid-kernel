#ifndef IPHONE_NAND_H
#define IPHONE_NAND_H
#include <linux/io.h>

typedef struct NANDFTLData {
	u16 sysSuBlks;
	u16 field_2;
	u16 field_4;		// reservoir blocks?
	u16 field_6;
	u16 field_8;
} NANDFTLData;

typedef struct SpareData {
	union {
		struct {
			u32 logicalPageNumber;
			u32 usn;
		} __attribute__ ((packed)) user;

		struct {
			u32 usnDec;
			u16 idx;
			u8 field_6;
			u8 field_7;
		} __attribute__ ((packed)) meta;
	};

	u8 type2;
	u8 type1;
	u8 eccMark;
	u8 field_B;

} __attribute__ ((packed)) SpareData;

typedef struct NANDData {
	u32 field_0;
	u16 field_4;
	u16 sectorsPerPage;
	u16 pagesPerBlock;
	u16 pagesPerSuBlk;
	u32 pagesPerBank;
	u32 pagesTotal;
	u16 suBlksTotal;
	u16 userSuBlksTotal;
	u32 userPagesTotal;
	u16 blocksPerBank;
	u16 bytesPerPage;
	u16 bytesPerSpare;
	u16 field_22;
	u32 field_24;
	u32 DeviceID;
	u16 banksTotal;
	u8 field_2E;
	u8 field_2F;
	int* banksTable;
} NANDData;

#define SECTOR_SIZE 512

#define ERROR_EMPTYBLOCK 1
#define ERROR_ECC 2

extern struct platform_device iphone_nand;

int nand_read(int bank, int page, u8* buffer, u8* spare, bool doECC, bool checkBlank);
int nand_read_multiple(u16* bank, u32* pages, u8* main, SpareData* spare, int pagesCount);
int nand_read_alternate_ecc(int bank, int page, u8* buffer);
int nand_erase(int bank, int block);
int nand_write(int bank, int page, u8* buffer, u8* spare, bool doECC);
int nand_bank_reset(int bank, int timeout);
NANDFTLData* nand_get_ftl_data(void);
NANDData* nand_get_geometry(void);
int nand_setup(void);

#endif
