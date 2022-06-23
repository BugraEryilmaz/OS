#include "ls.h"

namespace ls {
    void printFileName(std::wstring name) { wprintf(L"%ls ", name.c_str()); }

    void printFolder(FatFileEntry *folder) {
        while (folder->msdos.filename[0] != 0x00) {
            if (folder->msdos.filename[0] == 0xE5) {
                folder = nextFileEntry(folder);
                if (!folder) break;
                continue;
            }
            fileInfo ret = getFileInfo(folder);
            printFileName(ret.filename);
            folder = ret.nextFile;
            if (!folder) break;
        }
        wprintf(L"\n");
    }
};
namespace ll {
    void printFileName(fileInfo &name) {
        if (name.fatFileInfo->attributes & FOLDERMASK) {
            // folder
            wprintf(L"drwx------ 1 root root 0 ");
        } else {
            // file
            wprintf(L"-rwx------ 1 root root %u ", name.fatFileInfo->fileSize);
        }
        char months[][4] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        wprintf(L"%hs %.2u %.2u:%.2u ", months[GETMONTH(name.fatFileInfo->modifiedDate)],
                GETDAY(name.fatFileInfo->modifiedDate), GETHOUR(name.fatFileInfo->modifiedTime),
                GETMIN(name.fatFileInfo->modifiedTime));
        wprintf(L"%ls\n", name.filename.c_str());
    }

    void printFolder(FatFileEntry *folder) {
        while (folder->msdos.filename[0] != 0x00) {
            if (folder->msdos.filename[0] == 0xE5) {
                folder = nextFileEntry(folder);
                if (!folder) break;
                continue;
            }
            fileInfo ret = getFileInfo(folder);
            printFileName(ret);
            folder = ret.nextFile;
            if (!folder) break;
        }
    }
};