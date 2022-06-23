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
        // std::cerr << "Error: source or destination is NULL" << std::endl;
        return;
    }
    if (src == dst) {
        // std::cerr << "Error: source and destination are the same" << std::endl;
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
        // std::cerr << "Error: file not found" << std::endl;
        return;
    }

    bool mksuccess =
        mkfile_folder(dst, fileName, info.fatFileInfo->attributes & FOLDERMASK,
                      info.fatFileInfo->eaIndex << 16 | info.fatFileInfo->firstCluster, info.fatFileInfo->attributes,
                      info.fatFileInfo->creationTime, info.fatFileInfo->creationDate, info.fatFileInfo->creationTimeMs,
                      info.fatFileInfo->fileSize);
    if (!mksuccess) {
        // std::cerr << "Error: failed to create file possibly duplicate" << std::endl;
        return;
    }
    // delete the old file
    deleteFile(iter);
}