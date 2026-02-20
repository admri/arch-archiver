#include <arch/unarchiver.h>

#include "core/archive.h"
#include "core/archive_header.h"
#include "core/file_header.h"
#include "util/file.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

ArchResult arch_open(const char* path, Archive** outArchive)
{
    if (!path || !outArchive)
        return ARCH_ERR_INVALID_ARGUMENT;

    *outArchive = NULL;
    Archive* archive = createArchive(path, "rb");
    if (!archive)
        return ARCH_ERR_IO;

    ArchiveHeader header;
    if (!readArchiveHeader(archive->file, &header))
    {
        freeArchive(archive);
        return ARCH_ERR_IO;
    }

    if (header.magic != ARCH_MAGIC)
    {
        freeArchive(archive);
        return ARCH_ERR_NOT_AN_ARCHIVE;
    }

    if (header.version > ARCH_VERSION)
    {
        freeArchive(archive);
        return ARCH_ERR_UNSUPPORTED_VERSION;
    }

    archive->fileCount = header.fileCount;
    archive->currentFileIndex = 0;
    archive->readOnly = true;

    *outArchive = archive;
    return ARCH_OK;
}

ArchResult arch_retrieveNextFile(Archive* archive, const char* output_dir)
{
    if (!archive || !output_dir)
        return ARCH_ERR_INVALID_ARGUMENT;

    if (archive->currentFileIndex >= archive->fileCount)
        return ARCH_ERR_INVALID_ARGUMENT;
    
    char* fileName = NULL;
    char* filePath = NULL;
    FILE* file = NULL;
    ArchResult result = ARCH_OK;

    FileHeader header;
    if (!readFileHeader(archive->file, &header, &fileName))
    {
        result = ARCH_ERR_IO;
        goto cleanup;
    }

    if (header.magic != ARCH_FILE_MAGIC)
    {
        result = ARCH_ERR_CORRUPTED;
        goto cleanup;
    }

    size_t filePathSize = strlen(output_dir) + sizeof(DIR_SEP) + strlen(fileName) + 1;
    
    filePath = malloc(filePathSize);
    if (!filePath)
    {
        result = ARCH_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }

    snprintf(filePath, filePathSize, "%s%c%s", output_dir, DIR_SEP, fileName);

    if (!createParentDirectories(filePath))
    {
        result = ARCH_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }

    file = fopen(filePath, "wb");
    if (!file)
    {
        result = ARCH_ERR_IO;
        goto cleanup;
    }

    if (header.flags & ARCH_FLAG_COMPRESSED)
    {
        uint32_t crcUncompressed = 0;
        uint32_t crcCompressed = 0;

        ArchResult decompResult = decompressFileStream(archive->file, file, header.compSize, &crcUncompressed, &crcCompressed);
        
        if (decompResult != ARCH_OK)
        {
            result = decompResult;
            goto cleanup;
        }

        if (crcUncompressed != header.crc32_uncompressed ||
            crcCompressed != header.crc32_compressed)
        {
            result = ARCH_ERR_CORRUPTED;
            goto cleanup;
        }
    }
    else
    {
        uint32_t crc = 0;

        if (!copyFileData(archive->file, file, header.origSize, &crc))
        {
            result = ARCH_ERR_IO;
            goto cleanup;
        }
        
        if (crc != header.crc32_uncompressed)
        {
            result = ARCH_ERR_CORRUPTED;
            goto cleanup;
        }
    }

cleanup:
    free(filePath);
    free(fileName);
    if (file) fclose(file);

    archive->currentFileIndex++;
    return result;
}

size_t arch_getFileCount(Archive *archive)
{
    if (!archive) return 0;
    return archive->fileCount;
}
