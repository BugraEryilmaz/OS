#include "cd.h"

void changeDir(std::wstring dir) {
    FatFileEntry *file = getFolder(dir);
    if (file == nullptr) {
        return;
    } else {
        if (dir[0] == L'/') {
            currentDirStr = dir;
        } else {
            if (*currentDirStr.rbegin() != '/') currentDirStr.push_back(L'/');
            currentDirStr.append(dir);
            // simplify ..'s in dir str
            do {
                size_t pos = currentDirStr.find(L"/..");
                if (pos == std::string::npos) break;
                size_t pos2 = currentDirStr.rfind(L'/', pos - 1);
                if (pos2 == std::string::npos) break;
                currentDirStr.erase(pos2, pos - pos2 + 3);
                // if (currentDirStr.back() == '/') currentDirStr.pop_back();
            } while (true);
            // simplify .'s in dir str

            do {
                size_t pos = currentDirStr.find(L"/.");
                if (pos == std::string::npos) break;
                currentDirStr.erase(pos, 2);
                // if (currentDirStr.back() == '/') currentDirStr.pop_back();
            } while (true);
        }
        currentDir = file;
    }
}