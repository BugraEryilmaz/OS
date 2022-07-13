#pragma once
#include "fat32helper.h"
#include <chrono>
#include <math.h>

extern bool mkfile_folder(FatFileEntry *folder, std::wstring &fileName, bool isDir, uint32_t cluster = 0,
                          uint8_t attributes = UINT8_MAX, uint16_t time = UINT16_MAX, uint16_t date = UINT16_MAX,
                          uint8_t ms = UINT8_MAX, uint32_t size = 0);