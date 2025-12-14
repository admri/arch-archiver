#ifndef ARCHIVE_HEADER_H
#define ARCHIVE_HEADER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ARCHIVE_MAGIC "ARCH"
#define ARCHIVE_VERSION 1

typedef struct ArchiveHeader
{
    char magic[4];
    uint16_t version;
    uint32_t fileCount;
    char reserved[20];
} ArchiveHeader; // 32 bytes

bool createArchiveHeader(ArchiveHeader* header);
void freeArchiveHeader(ArchiveHeader* header);

bool writeArchiveHeader(FILE* file, const ArchiveHeader* header);
bool updateArchiveHeaderFileCount(FILE* file, uint32_t fileCount);
bool readArchiveHeader(FILE* file, ArchiveHeader* header);

#endif // ARCHIVE_HEADER_H
