#pragma once
#include "fat32.h"
#include <iostream>
#include <string.h>

#define FOLDERMASK 0x10
#define FOLDERATTR 0x10
#define FILEATTR 0x00
#define GETMONTH(dateParam) (((dateParam)&0b0000000111100000) >> 5)
#define GETDAY(dateParam) ((dateParam)&0b0000000000011111)
#define GETHOUR(timeParam) (((timeParam)&0b1111100000000000) >> 11)
#define GETMIN(timeParam) (((timeParam)&0b0000011111100000) >> 5)
#define GETDATACLUSTER(fatfile83) (((fatfile83).eaIndex << 16) + ((fatfile83).firstCluster))

extern BPB_struct *biosParameterBlock;
extern uint32_t *FATbeginning;
extern uint32_t FAToffset;
extern uint8_t *DATAbeginning;
extern uint32_t SECTOR_SIZE;
extern uint32_t CLUSTER_SIZE;
extern std::wstring currentDirStr;
extern struct_FatFileEntry *currentDir;

struct fileInfo {
    std::wstring filename;
    FatFile83 *fatFileInfo;
    FatFileEntry *nextFile;
};

void initialize(void *img);
void *getNextCluster(void *ptr);
FatFileEntry *nextFileEntry(FatFileEntry *cur);
fileInfo getFileInfo(FatFileEntry *current);
struct_FatFileEntry *getFolder(std::wstring newLocation);
void *allocateNewCluster(void *cur);
unsigned char lfn_checksum(const unsigned char *pFCBName);
bool isLFN(FatFileEntry *current);