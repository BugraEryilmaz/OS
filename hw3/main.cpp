#include "cat.h"
#include "cd.h"
#include "fat32helper.h"
#include "ls.h"
#include "mkfile-folder.h"
#include "mv.h"
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
        std::wstring command;
        std::wcin >> command;
        wprintf(L"%ls", command.c_str());
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