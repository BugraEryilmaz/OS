#pragma once
#include "fat32helper.h"

namespace ls {
    void printFileName(std::wstring name);

    void printFolder(FatFileEntry *folder);
};
namespace ll {
    void printFileName(fileInfo &name);

    void printFolder(FatFileEntry *folder);
};