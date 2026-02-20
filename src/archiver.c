#include <arch/archiver.h>

#include "core/archive.h"
#include "core/archive_header.h"
#include "core/file_header.h"
#include "util/file.h"

#include <stdlib.h>

ArchResult arch_create(const char *path, Archive** outArchive)
{
    if (!path || !outArchive)
        return ARCH_ERR_INVALID_ARGUMENT;

    *outArchive = NULL;

    Archive* archive = createArchive(path, "wb+");
    if (!archive)
        return ARCH_ERR_IO;

    ArchiveHeader header;
    if (!createArchiveHeader(&header))
    {
        freeArchive(archive);
        return ARCH_ERR_INTERNAL;
    }

    if (!writeArchiveHeader(archive->file, &header))
    {
        freeArchive(archive);
        return ARCH_ERR_IO;
    }

    *outArchive = archive;
    return ARCH_OK;
}

ArchResult arch_addFile(Archive* archive, const char* path)
{
    if (!archive || !path)
        return ARCH_ERR_INVALID_ARGUMENT;

    FILE* file = NULL;
    char* fileName = NULL;

    FileHeader fileHeader;
    uint64_t fileSize = 0;

    if (!createFileHeader(path, ARCH_FLAG_COMPRESSED, &fileHeader, &file, &fileSize))
        return ARCH_ERR_IO;

    fileName = getFileName(path, false);
    if (!fileName)
        goto oom;

    uint64_t compSizePos = 0;
    uint64_t crcUncompressedPos = 0;
    uint64_t crcCompressedPos = 0;

    if (!writeFileHeader(archive->file, &fileHeader, fileName, &compSizePos, &crcUncompressedPos, &crcCompressedPos))
        goto io_fail;

    if (fileHeader.flags & ARCH_FLAG_COMPRESSED)
    {
        uint64_t compSize = 0;
        uint32_t crcUncompressed = 0;
        uint32_t crcCompressed = 0;

        if (!compressFileStream(file, archive->file, &compSize, &crcUncompressed, &crcCompressed))
            goto compress_fail;

        if (!updateFileHeaderCompSize(&fileHeader, archive->file, compSizePos, compSize))
            goto io_fail;

        if (!updateFileHeaderCRC32(&fileHeader, archive->file, crcUncompressedPos, crcCompressedPos, crcUncompressed, crcCompressed))
            goto io_fail;
    }
    else
    {
        uint32_t crc = 0;

        if (!copyFileData(file, archive->file, fileSize, &crc))
            goto io_fail;

        if (!updateFileHeaderCRC32(&fileHeader, archive->file, crcUncompressedPos, crcCompressedPos, crc, crc))
            goto io_fail;
    }

    archive->fileCount++;

    fclose(file);
    free(fileName);
    return ARCH_OK;

oom:
    fclose(file);
    free(fileName);
    return ARCH_ERR_OUT_OF_MEMORY;

compress_fail:
    fclose(file);
    free(fileName);
    return ARCH_ERR_COMPRESSION;

io_fail:
    fclose(file);
    free(fileName);
    return ARCH_ERR_IO;
}

void arch_close(Archive* archive)
{
    if (!archive) return;

    if (!archive->readOnly)
    {
        updateArchiveHeaderFileCount(archive->file, archive->fileCount);
    }

    freeArchive(archive);
}
