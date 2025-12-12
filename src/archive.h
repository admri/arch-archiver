#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "file.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Archive
{
    const char* filePath;
    FILE* file;
    char** fileList;
    size_t fileCount;
};

struct ArchiveHeader
{
    char magic[4];
    uint16_t version;
    uint32_t fileCount;
    char reserved[20];
}; // 32 bytes

struct FileHeader
{
    uint16_t nameLength;
    uint64_t origSize;
    uint64_t compSize;
    uint8_t flags;
}; // 19 bytes (+ variable-sized file name)

bool writeArchiveHeader(FILE* file, struct ArchiveHeader* header);
bool createArchive(struct Archive* archive, struct ArchiveHeader* header);

bool writeFileHeader(FILE* file, struct FileHeader* header, const char* fileName, long* compSizePos);
bool addToArchive(FILE* archive, const char* filePath);

#endif // ARCHIVE_H
