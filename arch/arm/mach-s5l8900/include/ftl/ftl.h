#ifndef IPHONE_FTL_H
#define IPHONE_FTL_H

int ftl_setup(void);
bool ftl_sync(void);
int FTL_Write(u32 logicalPageNumber, int totalPagesToWrite, u8* pBuf);
int FTL_Read(u32 logicalPageNumber, int totalPagesToRead, u8* pBuf);

#endif
