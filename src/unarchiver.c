#include <arch/unarchiver.h>

#include "core/archive.h"
#include "core/archive_header.h"
#include "core/file_header.h"
#include "util/file.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Archive* arch_open(const char* path)
{
    if (!path) return NULL;

    Archive* archive = NULL;

    archive = createArchive(path, "rb");
    if (!archive) return NULL;

    ArchiveHeader header;
    if (!readArchiveHeader(archive->file, &header) || memcmp(header.magic, ARCHIVE_MAGIC, sizeof(header.magic)) != 0)
    {
        freeArchive(archive);
        return NULL;
    }

    archive->fileCount = header.fileCount;

    return archive;
}

bool arch_retrieveNextFile(Archive* archive, const char* output_dir)
{
    if (!archive || !output_dir) return false;
    if (archive->currentFileIndex >= archive->fileCount) return false;
    
    char* fileName = NULL;
    char* filePath = NULL;
    FILE* file = NULL;
    bool ok = false;

    FileHeader header;
    if (!readFileHeader(archive->file, &header, &fileName)) goto cleanup;

    size_t filePathSize = strlen(output_dir) + sizeof(DIR_SEP) + strlen(fileName) + 1;
    filePath = malloc(filePathSize);
    if (!filePath) goto cleanup;

    snprintf(filePath, filePathSize, "%s%c%s", output_dir, DIR_SEP, fileName);

    file = fopen(filePath, "wb");
    if (!file)
    {
        perror("Failed to open output file");
        goto cleanup;
    }

    if (header.flags & COMPRESSED_FLAG)
    {
        uint32_t crcUncompressed = 0;
        uint32_t crcCompressed = 0;
        ok = decompressFileStream(archive->file, file, header.compSize, &crcUncompressed, &crcCompressed);
    }
    else
    {
        uint32_t crc = 0;
        ok = copyFileData(archive->file, file, header.origSize, &crc);
    }

cleanup:
    free(filePath);
    free(fileName);
    if (file) fclose(file);

    archive->currentFileIndex++;
    return ok;
}

size_t arch_getFileCount(Archive *archive)
{
    if (!archive) return 0;
    return archive->fileCount;
}
