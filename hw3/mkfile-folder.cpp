#include "mkfile-folder.h"

uint32_t requiredFileEntries(std::wstring dirName) { return std::ceil(dirName.size() / 13.0) + 1; }

std::string format83filename(std::wstring &filename, uint32_t lastNumber) {
    std::wstring filenameNoExt = filename.substr(0, filename.find_last_of(L'.'));
    std::string name83 = "           ";
    for (size_t i = 0; i < 6; i++) {
        if (i < filenameNoExt.size()) {
            name83[i] = filename[i];
        }
    }
    if (filename.size() <= 6) {
        return name83;
    }
    uint32_t number = lastNumber;
    uint32_t numberidx = 7;
    do {
        uint32_t rem = number % 10;
        name83[numberidx] = rem + '0';
        number = number / 10;
        numberidx--;
    } while (number && numberidx > 0);
    name83[numberidx] = '~';

    if (filename.find_last_of(L'.') != std::wstring::npos) {
        std::wstring extension = filename.substr(filename.find_last_of(L'.') + 1);
        for (size_t i = 0; i < 3; i++) {
            if (i < extension.size()) {
                name83[i + 8] = extension[i];
            }
        }
    }
    return name83;
}

void fillLFN(FatFileEntry *currentEntry, uint32_t currentLFN, uint32_t checksum, uint32_t numberOfLFN) {
    // the previous LFN was finished fill its remaining fields
    // calculate checksum for the current LFN
    currentEntry->lfn.checksum = checksum;
    if (currentLFN == numberOfLFN - 1) {
        // seq number is 0x4lfn for last LFN
        currentEntry->lfn.sequence_number = (currentLFN + 1) | 0x40;
    } else {
        // seq number is lfn for other LFNs
        currentEntry->lfn.sequence_number = currentLFN + 1;
    }
    currentEntry->lfn.attributes = 0x0F;
    currentEntry->lfn.firstCluster = 0;
    currentEntry->lfn.reserved = 0;
}

void fill83File(FatFileEntry *currentEntry, std::string filename, uint32_t cluster, uint8_t attributes,
                uint16_t time = UINT16_MAX, uint16_t date = UINT16_MAX, uint8_t ms = UINT8_MAX, uint32_t size = 0) {
    auto highResTime = std::chrono::high_resolution_clock::now();
    time_t rawTime = std::time(NULL);
    struct tm *timeInfo = gmtime(&rawTime);
    uint8_t seconds =
        (std::chrono::duration_cast<std::chrono::seconds>(highResTime.time_since_epoch()).count() % 60) / 2;
    uint8_t minutes = std::chrono::duration_cast<std::chrono::minutes>(highResTime.time_since_epoch()).count() % 60;
    uint8_t hours = std::chrono::duration_cast<std::chrono::hours>(highResTime.time_since_epoch()).count() % 24;
    if (time == UINT16_MAX) time = (hours << 11) | (minutes << 5) | seconds;
    uint16_t modtime = (hours << 11) | (minutes << 5) | seconds;
    if (date == UINT16_MAX) date = (timeInfo->tm_year - 80) << 9 | (timeInfo->tm_mon + 1) << 5 | timeInfo->tm_mday;
    uint16_t moddate = (timeInfo->tm_year - 80) << 9 | (timeInfo->tm_mon + 1) << 5 | timeInfo->tm_mday;
    if (ms == UINT16_MAX)
        ms =
            (std::chrono::duration_cast<std::chrono::milliseconds>(highResTime.time_since_epoch()).count() % 2000) / 10;

    currentEntry->msdos.attributes = attributes;
    currentEntry->msdos.reserved = 0;
    currentEntry->msdos.creationTime = time;
    currentEntry->msdos.creationTimeMs = ms;
    currentEntry->msdos.creationDate = date;
    currentEntry->msdos.lastAccessTime = modtime;
    currentEntry->msdos.modifiedDate = moddate;
    currentEntry->msdos.modifiedTime = modtime;
    currentEntry->msdos.eaIndex = cluster >> 16;
    currentEntry->msdos.firstCluster = cluster & 0xFFFF;
    currentEntry->msdos.fileSize = size;
    if (filename[0] == 0xe5) {
        currentEntry->msdos.filename[0] = 0x05;
    } else {
        currentEntry->msdos.filename[0] = filename[0];
    }
    for (size_t i = 1; i < 11; i++) {
        if (i < 8) {
            if (i < filename.size()) {
                currentEntry->msdos.filename[i] = filename[i];
            } else {
                currentEntry->msdos.filename[i] = ' ';
            }
        } else {
            if (i < filename.size()) {
                currentEntry->msdos.extension[i - 8] = filename[i];
            } else {
                currentEntry->msdos.extension[i - 8] = ' ';
            }
        }
    }
}

void createFileEntries(FatFileEntry *dirLocation, std::wstring &dirName, uint32_t lastNumber, FatFileEntry *folder,
                       bool isDir, uint32_t cluster = 0, uint8_t attributes = UINT8_MAX, uint16_t time = UINT16_MAX,
                       uint16_t date = UINT16_MAX, uint8_t ms = UINT8_MAX, uint32_t size = 0) {
    uint32_t numberOfLFN = ceil(dirName.size() / 13.0);
    uint32_t numberOfEntries = numberOfLFN + 1;
    uint32_t currentLFN = numberOfLFN - 1;
    FatFileEntry *currentEntry = dirLocation;
    // dirname has no extension since it is a folder
    std::string name83 = format83filename(dirName, lastNumber);

    uint8_t checksum = lfn_checksum((unsigned char *)name83.c_str());
    for (int i = numberOfLFN * 13 - 1; i >= 0; i--) {
        uint32_t idx = i % 13;
        uint32_t lfn = i / 13;
        wchar_t charToStore = 0xFFFF;
        if (i == dirName.size()) {
            charToStore = 0x0000;
        } else if (i < dirName.size()) {
            charToStore = dirName[i];
        }
        if (lfn != currentLFN) {
            fillLFN(currentEntry, currentLFN, checksum, numberOfLFN);
            currentLFN = lfn;
            FatFileEntry *nextEntry = nextFileEntry(currentEntry);
            if (nextEntry == nullptr) {
                // we are at the end of the folder, allocate a new cluster
                nextEntry = (FatFileEntry *)allocateNewCluster(currentEntry);
            }
            currentEntry = nextEntry;
        }

        if (idx < 5)
            currentEntry->lfn.name1[idx] = charToStore;
        else if (idx < 11)
            currentEntry->lfn.name2[idx - 5] = charToStore;
        else if (idx < 13)
            currentEntry->lfn.name3[idx - 11] = charToStore;
    }
    // the previous LFN was finished fill its remaining fields
    fillLFN(currentEntry, currentLFN, checksum, numberOfLFN);
    FatFileEntry *nextEntry = nextFileEntry(currentEntry);
    if (nextEntry == nullptr) {
        // we are at the end of the folder, allocate a new cluster
        nextEntry = (FatFileEntry *)allocateNewCluster(currentEntry);
    }
    currentEntry = nextEntry;

    if (isDir) {
        // fill fat83 file
        if (cluster == 0) {
            FatFileEntry *allocatedFolder = (FatFileEntry *)allocateNewCluster(nullptr);
            cluster = (((uint8_t *)allocatedFolder) - DATAbeginning) / CLUSTER_SIZE + 2;
            // add . and .. entries into new folder
            uint32_t dotEntryCluster = currentEntry->msdos.firstCluster | (currentEntry->msdos.eaIndex << 16);
            uint32_t dotdotEntryCluster = (((uint8_t *)folder) - DATAbeginning) / CLUSTER_SIZE + 2;
            if (dotdotEntryCluster == biosParameterBlock->extended.RootCluster) {
                dotdotEntryCluster = 0;
            }
            fill83File(allocatedFolder, ".", dotEntryCluster, FOLDERATTR);
            fill83File(nextFileEntry(allocatedFolder), "..", dotdotEntryCluster, FOLDERATTR);
        } else {
            // update .. entry so that it points to this folder
            FatFileEntry *allocatedFolder = (FatFileEntry *)DATAbeginning + (cluster - 2) * CLUSTER_SIZE;
            uint32_t dotdotEntryCluster = (((uint8_t *)folder) - DATAbeginning) / CLUSTER_SIZE + 2;
            if (dotdotEntryCluster == biosParameterBlock->extended.RootCluster) {
                dotdotEntryCluster = 0;
            }
            FatFileEntry *iter = allocatedFolder;
            fileInfo info;
            do {
                if (iter->msdos.filename[0] == 0x00) {
                    // all subsequent entries are available and file not found
                    fill83File(iter, "..", dotdotEntryCluster, FOLDERATTR);
                    break;
                } else if (iter->msdos.filename[0] == 0xE5) {
                    // deleted entry check next
                    iter = nextFileEntry(iter);
                    continue;
                } else {
                    info = getFileInfo(iter);
                    // the same file is found exit
                    if (info.filename.compare(L"..") == 0) {
                        fill83File((FatFileEntry *)info.fatFileInfo, "..", dotdotEntryCluster, FOLDERATTR);
                        break;
                    }
                    iter = info.nextFile;
                }
            } while (iter);
            if (!iter) {
                // end of cluster with no .. entry create a new cluster and put .. into it
                FatFileEntry *allocatedFolder = (FatFileEntry *)allocateNewCluster(nullptr);
                cluster = (((uint8_t *)allocatedFolder) - DATAbeginning) / CLUSTER_SIZE + 2;
                fill83File(allocatedFolder, "..", dotdotEntryCluster, FOLDERATTR);
            }
            fill83File(nextFileEntry(allocatedFolder), "..", dotdotEntryCluster, FOLDERATTR);
        }
        if (attributes == UINT8_MAX) attributes = FOLDERATTR;
        fill83File(currentEntry, name83, cluster, attributes, time, date, ms, size);

    } else {
        if (attributes == UINT8_MAX) attributes = FILEATTR;
        fill83File(currentEntry, name83, cluster, attributes, time, date, ms, size);
    }
}

bool mkfile_folder(FatFileEntry *folder, std::wstring &fileName, bool isDir, uint32_t cluster = 0,
                   uint8_t attributes = UINT8_MAX, uint16_t time = UINT16_MAX, uint16_t date = UINT16_MAX,
                   uint8_t ms = UINT8_MAX, uint32_t size = 0) {
    uint32_t lastNumberFormat = 0;
    uint32_t numberOfEntry = requiredFileEntries(fileName);
    uint32_t curNoEmptyEntries = 0;
    FatFileEntry *iter = folder;
    FatFileEntry *fileLocation = nullptr;
    FatFileEntry *maybeFileLocation = nullptr;
    // search for the first enough space in the folder
    do {
        FatFileEntry *nextFolder = nullptr;
        if (iter->msdos.filename[0] == 0x00) {
            // all subsequent entries are available
            if (!fileLocation) fileLocation = iter;
            break;
        } else if (iter->msdos.filename[0] == 0xE5) {
            // deleted entry start counting empty entries
            curNoEmptyEntries++;
            if (!maybeFileLocation) maybeFileLocation = iter;
            // if we found enough empty entries select this as dir location
            if (curNoEmptyEntries >= numberOfEntry && !fileLocation) {
                fileLocation = maybeFileLocation;
            }
            // continue to check other entries since we may have already the same folder
            nextFolder = nextFileEntry(iter);
        } else {
            curNoEmptyEntries = 0;
            maybeFileLocation = nullptr;
            fileInfo info = getFileInfo(iter);
            // the same file is found exit
            if (info.filename.compare(fileName) == 0) return false;
            // the files share the same first 6 chars increment the file~idx for fat8.3
            if (info.filename.compare(0, 5, fileName, 0, 5) == 0) {
                lastNumberFormat++;
            }
            nextFolder = info.nextFile;
        }
        // end of cluster chain is reached. allocate new cluster if we could not found our dirlocation
        if (nextFolder == nullptr) {
            if (fileLocation) break;
            // allocate new cluster if the last elements were not deleted
            if (maybeFileLocation)
                fileLocation = maybeFileLocation;
            else
                fileLocation = (FatFileEntry *)allocateNewCluster(iter);
            break;
        }
        iter = nextFolder;
    } while (1);
    createFileEntries(fileLocation, fileName, lastNumberFormat, folder, isDir, cluster, attributes, time, date, ms,
                      size);
    return true;
}