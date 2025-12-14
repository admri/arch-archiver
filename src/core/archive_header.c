#include "archive_header.h"
#include "../util/file.h"

#include <stdlib.h>
#include <string.h>

bool createArchiveHeader(ArchiveHeader* header)
{
    if (!header) return false;

    memcpy(header->magic, ARCHIVE_MAGIC, sizeof(header->magic));
    header->version = ARCHIVE_VERSION;
    header->fileCount = 0;
    memset(header->reserved, 0, sizeof(header->reserved));

    return true;
}

void freeArchiveHeader(ArchiveHeader *header)
{
    if (header) free(header);
}

bool writeArchiveHeader(FILE* file, const ArchiveHeader* header)
{
    if (!writeFile(file, header->magic, sizeof(header->magic))) return false;
    if (!writeFile(file, (const char*)&header->version, sizeof(header->version))) return false;
    if (!writeFile(file, (const char*)&header->fileCount, sizeof(header->fileCount))) return false;
    if (!writeFile(file, header->reserved, sizeof(header->reserved))) return false;
    return true;
}

bool updateArchiveHeaderFileCount(FILE* file, uint32_t fileCount)
{
    int64_t origPos = ftell64(file);
    if (origPos < 0) return false;

    if (fseek(file, 6, SEEK_SET) != 0) return false;
    if (!writeFile(file, (const char*)&fileCount, sizeof(fileCount))) return false;
    fflush(file);

    if (fseek64(file, origPos, SEEK_SET) != 0) return false;

    return true;
}

bool readArchiveHeader(FILE* file, ArchiveHeader* header)
{
    if (!header || !file) return false;
    
    size_t read;

    // Magic number
    unsigned char magic[sizeof header->magic];

    readFile(file, magic, sizeof magic, &read);
    if (read != sizeof magic)
    {
        perror("Failed to read magic number");
        return false;
    }

    memcpy(header->magic, magic, sizeof magic);

    // Version number
    unsigned char version[sizeof header->version];

    readFile(file, version, sizeof version, &read);
    if (read != sizeof version)
    {
        perror("Failed to read archive version");
        return false;
    }

    header->version = read_u16_le(version);

    // Files count
    unsigned char fileCount[sizeof header->fileCount];

    readFile(file, fileCount, sizeof fileCount, &read);

    if (read != sizeof fileCount)
    {
        perror("Failed to read files count");
        return false;
    }

    header->fileCount = read_u32_le(fileCount);

    // Reserved bytes
    if (fseek(file, sizeof header->reserved, SEEK_CUR) != 0)
    {
        return false;
    }

    return true;
}
