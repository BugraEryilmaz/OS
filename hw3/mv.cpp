#include "mv.h"

void deleteFile(FatFileEntry *file) {
    FatFileEntry *iter = file;
    while (1) {
        if (isLFN(iter)) {
            iter->msdos.filename[0] = 0xE5;
            iter = nextFileEntry(iter);
        } else {
            iter->msdos.filename[0] = 0xE5;
            return;
        }
    }
}

void moveFile(FatFileEntry *src, FatFileEntry *dst, std::wstring fileName) {
    if (src == NULL || dst == NULL) {
        DEBUG_PRINT("Error: source or destination is NULL");
        return;
    }
    if (src == dst) {
        DEBUG_PRINT("Error: source and destination are the same");
        return;
    }
    FatFileEntry *iter = src;
    fileInfo info;
    do {
        if (iter->msdos.filename[0] == 0x00) {
            // all subsequent entries are available and file not found
            return;
        } else if (iter->msdos.filename[0] == 0xE5) {
            // deleted entry check next
            iter = nextFileEntry(iter);
            continue;
        } else {
            info = getFileInfo(iter);
            // the same file is found exit
            if (info.filename.compare(fileName) == 0) break;
            iter = info.nextFile;
        }
    } while (iter);

    if (!iter) {
        DEBUG_PRINT("Error: file not found");
        return;
    }

    bool mksuccess =
        mkfile_folder(dst, fileName, info.fatFileInfo->attributes & FOLDERMASK,
                      info.fatFileInfo->eaIndex << 16 | info.fatFileInfo->firstCluster, info.fatFileInfo->attributes,
                      info.fatFileInfo->creationTime, info.fatFileInfo->creationDate, info.fatFileInfo->creationTimeMs,
                      info.fatFileInfo->fileSize);
    if (!mksuccess) {
        DEBUG_PRINT("Error: failed to create file possibly duplicate");
        return;
    }
    // delete the old file
    deleteFile(iter);
}