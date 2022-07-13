#include "cat.h"
#include "cd.h"
#include "fat32helper.h"
#include "ls.h"
#include "mkfile-folder.h"
#include "mv.h"
#include "parser.h"
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <locale.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

void *addr;

void shell() {
    initialize(addr);
    while (true) {
        wprintf(L"%ls> ", currentDirStr.c_str());
        char command[1024] = "ls";
        std::cin.getline(command, 1024);
        std::string commandStr = command;
        if (commandStr.substr(0, 5) == "quit") break;

        parsed_input inp;
        parse(&inp, command);
        std::wstring wfirstArg = L"";
        std::wstring wsecondArg = L"";
        if (inp.arg1) {
            std::string firstArg = inp.arg1;
            wfirstArg.assign(firstArg.begin(), firstArg.end());
        }
        if (inp.arg2) {
            std::string secondArg = inp.arg2;
            wsecondArg.assign(secondArg.begin(), secondArg.end());
        }
        if (inp.type == CD) {
            changeDir(wfirstArg);
        } else if (inp.type == LS) {
            if (wfirstArg == L"") {
                // ls current dir
                ls::printFolder(currentDir);
            } else if (wfirstArg == L"-l" && wsecondArg == L"") {
                // ls -l current dir
                ll::printFolder(currentDir);
            } else if (wfirstArg == L"-l") {
                // ls -l dir
                FatFileEntry *file = getFolder(wsecondArg);
                if (file != nullptr) ll::printFolder(file);
            } else {
                // ls another dir
                FatFileEntry *file = getFolder(wfirstArg);
                if (file != nullptr) ls::printFolder(file);
            }
        } else if (inp.type == MKDIR) {
            size_t idxFolder = wfirstArg.find_last_of(L'/');
            FatFileEntry *folder = currentDir;
            std::wstring filename = wfirstArg;
            if (idxFolder != std::string::npos) {
                std::wstring srcFolder = wfirstArg.substr(0, idxFolder);
                if (srcFolder == L"") srcFolder = L"/";
                folder = getFolder(srcFolder);
                if (folder == nullptr) {
                    DEBUG_PRINT(srcFolder << " is not a folder");
                    clean_input(&inp);
                    continue;
                }
                filename = wfirstArg.substr(idxFolder + 1);
            }
            mkfile_folder(folder, filename, true);
        } else if (inp.type == TOUCH) {
            size_t idxFolder = wfirstArg.find_last_of(L'/');
            FatFileEntry *folder = currentDir;
            std::wstring filename = wfirstArg;
            if (idxFolder != std::string::npos) {
                std::wstring srcFolder = wfirstArg.substr(0, idxFolder);
                if (srcFolder == L"") srcFolder = L"/";
                folder = getFolder(srcFolder);
                if (folder == nullptr) {
                    DEBUG_PRINT(srcFolder << " is not a folder");
                    clean_input(&inp);
                    continue;
                }
                filename = wfirstArg.substr(idxFolder + 1);
            }
            mkfile_folder(folder, filename, false);
        } else if (inp.type == MV) {
            size_t idxFolder = wfirstArg.find_last_of(L'/');
            FatFileEntry *src = currentDir;
            std::wstring filename = wfirstArg;
            std::wstring srcFolder;
            if (idxFolder != std::string::npos) {
                srcFolder = wfirstArg.substr(0, idxFolder);
                if (srcFolder == L"") srcFolder = L"/";
                src = getFolder(srcFolder);
                if (src == nullptr) {
                    DEBUG_PRINT(srcFolder << " is not a folder");
                    clean_input(&inp);
                    continue;
                }
                filename = wfirstArg.substr(idxFolder + 1);
            }
            FatFileEntry *dst = getFolder(wsecondArg);
            if (dst == nullptr) {
                DEBUG_PRINT(wsecondArg << " is not a folder");
                clean_input(&inp);
                continue;
            }
            DEBUG_PRINT(L"copy " << filename << " from " << srcFolder << " to " << wsecondArg);
            moveFile(src, dst, filename);
        } else if (inp.type == CAT) {
            size_t idxFolder = wfirstArg.find_last_of(L'/');
            FatFileEntry *src = currentDir;
            std::wstring filename = wfirstArg;
            if (idxFolder != std::string::npos) {
                std::wstring srcFolder = wfirstArg.substr(0, idxFolder);
                if (srcFolder == L"") srcFolder = L"/";
                src = getFolder(srcFolder);
                if (src == nullptr) {
                    DEBUG_PRINT(srcFolder << " is not a folder");
                    clean_input(&inp);
                    continue;
                }
                filename = wfirstArg.substr(idxFolder + 1);
            }
            printFile(src, filename);
        } else {
            wprintf(L"Error: unknown command\n");
        }
        clean_input(&inp);
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    int fd;
    struct stat sb;
    size_t length;
    ssize_t s;

    if (argc < 2) {
        cerr << "FAT32 img argument is necessary! " << argc << "\n";
        cerr << argv[0] << "\n";
        cerr << argv[1] << "\n";
        exit(EXIT_FAILURE);
    }

    fd = open(argv[1], O_RDWR);
    if (fd == -1) cerr << "open", exit(-1);

    if (fstat(fd, &sb) == -1) /* To obtain file size */
        cerr << "fstat", exit(-1);

    length = sb.st_size;

    addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) cerr << "mmap", exit(-1);

    shell();

    munmap(addr, length);
    close(fd);

    exit(EXIT_SUCCESS);
}