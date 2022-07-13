#include "fat32helper.h"

BPB_struct *biosParameterBlock = nullptr;
FSInfo *FSinfoBlock = nullptr;
uint32_t *FATbeginning = nullptr;
uint32_t FAToffset = 0;
uint8_t *DATAbeginning = nullptr;
uint32_t SECTOR_SIZE = 0;
uint32_t CLUSTER_SIZE = 0;
std::wstring currentDirStr = L"/";
struct_FatFileEntry *currentDir = nullptr;

uint32_t FATidxConversion(uint32_t input) {
    // input is stored as little endian if the machine is not little endian we need to convert it
#if (BYTE_ORDER == BIG_ENDIAN)
    uint32_t ret = (input >> 24) | ((input << 8) & 0x00FF0000) | ((input >> 8) & 0x0000FF00) | (input << 24);
#else
    uint32_t ret = input;
#endif
    // mask the top 4 bits
    ret = ret & 0x0FFFFFFF;
    return ret;
}

void initialize(void *img) {
    biosParameterBlock = (BPB_struct *)img;
    SECTOR_SIZE = biosParameterBlock->BytesPerSector;
    CLUSTER_SIZE = SECTOR_SIZE * biosParameterBlock->SectorsPerCluster;
    uint16_t resSectors = biosParameterBlock->ReservedSectorCount;
    uint32_t resBytes = resSectors * SECTOR_SIZE;
    FATbeginning = (uint32_t *)(((uint8_t *)img) + resBytes);
    FAToffset = biosParameterBlock->extended.FATSize * SECTOR_SIZE;
    DATAbeginning = ((uint8_t *)FATbeginning) + biosParameterBlock->NumFATs * FAToffset;
    uint32_t rootCluster = biosParameterBlock->extended.RootCluster;
    currentDir = (FatFileEntry *)(DATAbeginning + CLUSTER_SIZE * (rootCluster - 2));
    FSinfoBlock = (FSInfo *)(((uint8_t *)img) + biosParameterBlock->extended.FSInfo * SECTOR_SIZE);
}

void *getNextCluster(void *ptr) {
    uint32_t cluster = (((uint8_t *)ptr) - DATAbeginning) / CLUSTER_SIZE + 2;
    uint32_t FATidx = FATidxConversion(FATbeginning[cluster]);
    if (FATidx >= 0x0FFFFFF8) return nullptr;
    void *ret = DATAbeginning + (FATidx - 2) * CLUSTER_SIZE;
    return ret;
}

FatFileEntry *nextFileEntry(FatFileEntry *cur) {
    uint32_t cluster = (((uint8_t *)cur) - DATAbeginning) / CLUSTER_SIZE + 2;
    FatFileEntry *next = cur + 1;
    FatFileEntry *begin = (FatFileEntry *)(DATAbeginning + CLUSTER_SIZE * (cluster - 2));
    // checks for End
    if ((uint8_t *)next - (uint8_t *)begin >= CLUSTER_SIZE) {
        // end of sector
        uint32_t nextCluster = FATidxConversion(FATbeginning[cluster]);
        if (nextCluster >= 0x0FFFFFF8) {
            // end of folder
            return nullptr;
        } else {
            // print the rest of directory
            cluster = nextCluster;
            next = (struct_FatFileEntry *)(DATAbeginning + CLUSTER_SIZE * (cluster - 2));
        }
    }
    return next;
}

bool isLFN(FatFileEntry *current) {
    if (current->lfn.attributes == 0x0f && current->lfn.reserved == 0x00 && current->lfn.firstCluster == 0x0000) {
        return true;
    } else {
        return false;
    }
}

fileInfo getFileInfoLong(FatFileEntry *current) {
    fileInfo ret = {L"", nullptr, nullptr};
    if (isLFN(current)) {
        ret = getFileInfoLong(nextFileEntry(current));
        for (size_t i = 0; i < 5; i++) {
            if (current->lfn.name1[i] == 0x0000) {
                return ret;
            }
            ret.filename.push_back(current->lfn.name1[i]);
        }
        for (size_t i = 0; i < 6; i++) {
            if (current->lfn.name2[i] == 0x0000) {
                return ret;
            }
            ret.filename.push_back(current->lfn.name2[i]);
        }
        for (size_t i = 0; i < 2; i++) {
            if (current->lfn.name3[i] == 0x0000) {
                return ret;
            }
            ret.filename.push_back(current->lfn.name3[i]);
        }
    } else {
        ret.fatFileInfo = &current->msdos;
        ret.nextFile = nextFileEntry(current);
    }
    return ret;
}
fileInfo getFileInfoShort(FatFileEntry *current) {
    fileInfo ret = {L"", &current->msdos, nextFileEntry(current)};
    if (current->msdos.filename[0] == 0x05) {
        ret.filename.push_back(0xE5);
    } else {
        ret.filename.push_back(current->msdos.filename[0]);
    }
    for (size_t i = 1; i < 8; i++) {
        if (current->msdos.filename[i] == ' ' || current->msdos.filename[i] == '\0') {
            break;
        }
        ret.filename.push_back(current->msdos.filename[i]);
    }
    if (current->msdos.extension[0] != ' ' && current->msdos.extension[0] != '\0') {
        ret.filename.push_back(L'.');
    }
    for (size_t i = 0; i < 3; i++) {
        if (current->msdos.extension[i] == ' ' || current->msdos.extension[i] == '\0') {
            break;
        }
        ret.filename.push_back(current->msdos.extension[i]);
    }
    return ret;
}

fileInfo getFileInfo(FatFileEntry *current) {
    // assumes it is a valid file entry
    if (isLFN(current))
        return getFileInfoLong(current);
    else
        return getFileInfoShort(current);
}

bool compFileName(struct_FatFileEntry *&iter, std::wstring match) {
    if (iter->msdos.filename[0] == 0xe5) {
        // erased entry continue with next
        iter = nextFileEntry(iter);
        return false;
    } else if (iter->msdos.filename[0] == 0x00) {
        // no more folder left return false;
        return false;
    }

    fileInfo thisFile = getFileInfo(iter);

    if (match.compare(thisFile.filename) == 0) {
        // they match
        iter = (FatFileEntry *)thisFile.fatFileInfo;
        return true;
    } else {
        iter = thisFile.nextFile;
        return false;
    }
}

struct_FatFileEntry *getFolder(std::wstring newLocation) {
    int idx = 0;
    FatFileEntry *iter = currentDir;
    if (newLocation[0] == '/') {
        iter = (FatFileEntry *)DATAbeginning;
        idx = 1;
    }
    while (idx < newLocation.size()) {
        uint32_t cluster;
        uint32_t currentcluster = (((uint8_t *)iter) - DATAbeginning) / CLUSTER_SIZE + 2;
        size_t nextIdx = newLocation.find_first_of('/', idx);
        if (nextIdx == std::wstring::npos) nextIdx = newLocation.size();
        std::wstring foldername = newLocation.substr(idx, nextIdx - idx);

        if (currentcluster == biosParameterBlock->extended.RootCluster && foldername == L".") {
            // Special case root has no . and .. entries so continue as if . is found and points to the root
            cluster = biosParameterBlock->extended.RootCluster;
        } else {
            do {
                if (iter->msdos.filename[0] == 0x00) {
                    // end of folder with no match return null
                    return nullptr;
                }
            } while (!compFileName(iter, foldername));
            // we found the file check if folder
            if (!(iter->msdos.attributes & FOLDERMASK)) return nullptr;
            // enter the folder
            cluster = GETDATACLUSTER(iter->msdos);
            if (cluster == 0) {
                // root has a special case its cluster is stored as 0. WHHHYYYY
                cluster = biosParameterBlock->extended.RootCluster;
            }
        }

        iter = (FatFileEntry *)(DATAbeginning + CLUSTER_SIZE * (cluster - 2));
        idx = nextIdx + 1;
    }
    return iter;
}

void FATtableWrite(uint32_t cluster, uint32_t val) {
    for (size_t i = 0; i < biosParameterBlock->NumFATs; i++) {
        // do not change the first 4 bits
        uint32_t valToBeWritten = FATidxConversion(val) | (FATbeginning[cluster + i * (FAToffset / 4)] & 0xF0000000);
        FATbeginning[cluster + i * (FAToffset / 4)] = valToBeWritten;
    }
}

void *allocateNewCluster(void *cur) {
    uint32_t allocatedCluster = 0;
    uint32_t searchStart = 2;
    if (cur != nullptr) {
        searchStart = (((uint8_t *)cur) - DATAbeginning) / CLUSTER_SIZE + 2;
    }
    for (size_t i = searchStart; i < FAToffset / 4; i++) {
        if (FATidxConversion(FATbeginning[i]) == 0) {
            allocatedCluster = i;
            break;
        }
    }
    FATtableWrite(allocatedCluster, 0x0FFFFFFF);
    if (cur != nullptr) {
        FATtableWrite(searchStart, allocatedCluster);
    }
    void *ret = DATAbeginning + CLUSTER_SIZE * (allocatedCluster - 2);
    memset(ret, 0, CLUSTER_SIZE);
    FSinfoBlock->freeClusters--;
    return ret;
}

unsigned char lfn_checksum(const unsigned char *pFCBName) {
    int i;
    unsigned char sum = 0;

    for (i = 11; i; i--)
        sum = ((sum & 1) << 7) + (sum >> 1) + *pFCBName++;

    return sum;
}
