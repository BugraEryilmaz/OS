#pragma once
#include "fat32helper.h"
#include "mkfile-folder.h"

void moveFile(FatFileEntry *src, FatFileEntry *dst, std::wstring fileName);