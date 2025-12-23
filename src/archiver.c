#include <arch/archiver.h>

#include "core/archive.h"
#include "core/archive_header.h"
#include "core/file_header.h"
#include "util/file.h"

#include <stdlib.h>

Archive* arch_create(const char *path)
{
    if (!path) return NULL;

    Archive* archive = createArchive(path, "wb+");
    if (!archive) return NULL;

    ArchiveHeader header;
    if (!createArchiveHeader(&header)) goto cleanup;

    if (!writeArchiveHeader(archive->file, &header)) goto cleanup;

    return archive;

cleanup:
    freeArchive(archive);
    return NULL;
}

bool arch_addFile(Archive* archive, const char* path)
{
    if (!archive || !path) return false;

    FILE* file = NULL;
    char* fileName = NULL;
    bool success = false;

    FileHeader fileHeader;
    uint64_t fileSize = 0;
    if (!createFileHeader(path, COMPRESSED_FLAG, &fileHeader, &file, &fileSize)) return false;

    fileName = getFileName(path, false);
    if (!fileName) goto cleanup;

    uint64_t compSizePos = 0;
    uint64_t crcUncompressedPos = 0;
    uint64_t crcCompressedPos = 0;
    if (!writeFileHeader(archive->file, &fileHeader, fileName, &compSizePos, &crcUncompressedPos, &crcCompressedPos)) goto cleanup;

    if (fileHeader.flags & COMPRESSED_FLAG)
    {
        uint64_t compSize = 0;
        uint32_t crcUncompressed = 0;
        uint32_t crcCompressed = 0;

        if (!compressFileStream(file, archive->file, &compSize, &crcUncompressed, &crcCompressed)) goto cleanup;
        if (!updateFileHeaderCompSize(&fileHeader, archive->file, compSizePos, compSize)) goto cleanup;
        if (!updateFileHeaderCRC32(&fileHeader, archive->file, crcUncompressedPos, crcCompressedPos, crcUncompressed, crcCompressed)) goto cleanup;
    }
    else
    {
        uint32_t crc = 0;
        if (!copyFileData(file, archive->file, fileSize, &crc)) goto cleanup;
        if (!updateFileHeaderCRC32(&fileHeader, archive->file, crcUncompressedPos, crcCompressedPos, crc, crc)) goto cleanup;
    }

    archive->fileCount++;
    if (!updateArchiveHeaderFileCount(archive->file, archive->fileCount)) goto cleanup;

    success = true;

cleanup:
    if (file) fclose(file);
    free(fileName);
    return success;
}

void arch_close(Archive* archive)
{
    if (archive) freeArchive(archive);
}
