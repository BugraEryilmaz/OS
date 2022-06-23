#pragma once
#include "fat32helper.h"
#include <chrono>
#include <math.h>

bool mkfile_folder(FatFileEntry *folder, std::wstring &fileName, bool isDir, uint32_t cluster, uint8_t attributes,
                   uint16_t time, uint16_t date, uint8_t ms, uint32_t size);