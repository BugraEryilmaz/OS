#include "cat.h"

// a function to print the file content
void printFile(FatFileEntry *file, std::wstring fileName) {
    if (file == NULL) {
        DEBUG_PRINT("Error: source is NULL");
        return;
    }
    FatFileEntry *iter = file;
    fileInfo info;
    while (1) {
        if (iter->msdos.filename[0] == 0)
            return;
        else if (iter->msdos.filename[0] == 0xE5)
            iter = nextFileEntry(iter);
        else {
            info = getFileInfo(iter);
            if (info.filename.compare(fileName) == 0) break;

            iter = info.nextFile;
            if (!iter) return;
        }
    }
    if (info.fatFileInfo->attributes & FOLDERMASK) {
        DEBUG_PRINT("Error: file is a folder");
        return;
    }
    // print the file content
    uint32_t cluster = info.fatFileInfo->firstCluster | (info.fatFileInfo->eaIndex << 16);
    if (cluster == 0) {
        DEBUG_PRINT("Error: file is empty");
        return;
    }
    char *buffer = (char *)DATAbeginning + (cluster - 2) * CLUSTER_SIZE;
    for (size_t i = 0; true; i++) {
        uint32_t idx = i % CLUSTER_SIZE;
        if (idx == 0 && i != 0) {
            buffer = (char *)getNextCluster(buffer);
            if (buffer == nullptr) break;
        }
        if (buffer[idx] == '\0' || buffer[idx] == EOF) break;
        wprintf(L"%c", buffer[idx]);
    }
    wprintf(L"\n");
}