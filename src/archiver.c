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

    ArchResult result = ARCH_OK;

    FILE* file = NULL;
    char* fileName = NULL;

    FileHeader fileHeader;
    uint64_t fileSize = 0;

    if (!createFileHeader(path, ARCH_FLAG_COMPRESSED, &fileHeader, &file, &fileSize))
        return ARCH_ERR_IO;

    fileName = sanitizeFilePath(path);
    if (!fileName)
    {
        result = ARCH_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }

    uint64_t compSizePos = 0;
    uint64_t crcUncompressedPos = 0;
    uint64_t crcCompressedPos = 0;

    if (!writeFileHeader(archive->file, &fileHeader, fileName, &compSizePos, &crcUncompressedPos, &crcCompressedPos))
    {
        result = ARCH_ERR_IO;
        goto cleanup;
    }

    if (fileHeader.flags & ARCH_FLAG_COMPRESSED)
    {
        uint64_t compSize = 0;
        uint32_t crcUncompressed = 0;
        uint32_t crcCompressed = 0;

        if (!compressFileStream(file, archive->file, &compSize, &crcUncompressed, &crcCompressed))
        {
            result = ARCH_ERR_COMPRESSION;
            goto cleanup;
        }

        if (!updateFileHeaderCompSize(&fileHeader, archive->file, compSizePos, compSize))
        {
            result = ARCH_ERR_IO;
            goto cleanup;
        }

        if (!updateFileHeaderCRC32(&fileHeader, archive->file, crcUncompressedPos, crcCompressedPos, crcUncompressed, crcCompressed))
        {
            result = ARCH_ERR_IO;
            goto cleanup;
        }
    }
    else
    {
        uint32_t crc = 0;

        if (!copyFileData(file, archive->file, fileSize, &crc))
        {
            result = ARCH_ERR_IO;
            goto cleanup;
        }

        if (!updateFileHeaderCRC32(&fileHeader, archive->file, crcUncompressedPos, crcCompressedPos, crc, crc))
        {
            result = ARCH_ERR_IO;
            goto cleanup;
        }
    }

    archive->fileCount++;

cleanup:
    fclose(file);
    free(fileName);
    return result;
}

ArchResult arch_addDirectory(Archive* archive, const char* dirPath)
{
    if (!archive || !dirPath)
        return ARCH_ERR_INVALID_ARGUMENT;

    ArchResult result = ARCH_OK;

#ifdef _WIN32
    struct _finddata_t findFileData;
    intptr_t hFind;
    char searchPath[1024];
    char pathBuffer[1024];

    snprintf(searchPath, sizeof(searchPath), "%s\\*", dirPath);

    hFind = _findfirst(searchPath, &findFileData);
    if (hFind == -1L)
    {
        return ARCH_ERR_IO;
    }

    do {
        // Skip "." and ".."
        if (strcmp(findFileData.name, ".") == 0 || strcmp(findFileData.name, "..") == 0)
        {
            continue;
        }

        // Build full path
        snprintf(pathBuffer, sizeof(pathBuffer), "%s\\%s", dirPath, findFileData.name);

        // Check if directory or file
        if (findFileData.attrib & _A_SUBDIR)
        {
            ArchResult r = arch_addDirectory(archive, pathBuffer);
            if (r != ARCH_OK) result = r;
        } 
        else
        {
            ArchResult r = arch_addFile(archive, pathBuffer);
            if (r != ARCH_OK)
            {
                fprintf(stderr, "Failed to add %s\n", pathBuffer);
                result = r;
            }
        }
    } while (_findnext(hFind, &findFileData) == 0);

    _findclose(hFind);

#else
    DIR* dir = opendir(dirPath);
    if (!dir) return ARCH_ERR_IO;

    struct dirent* entry;
    struct stat path_stat;
    char pathBuffer[1024]; 

    while ((entry = readdir(dir)) != NULL)
    {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Build full path
        snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", dirPath, entry->d_name);

        // Check if directory or file
        if (stat(pathBuffer, &path_stat) != 0) continue; 

        if (S_ISDIR(path_stat.st_mode))
        {
            ArchResult r = arch_addDirectory(archive, pathBuffer);
            if (r != ARCH_OK) result = r; 
        } 
        else if (S_ISREG(path_stat.st_mode))
        {
            ArchResult r = arch_addFile(archive, pathBuffer);
            if (r != ARCH_OK)
            {
                fprintf(stderr, "Failed to add %s\n", pathBuffer);
                result = r;
            }
        }
    }

    closedir(dir);
#endif

    return result;
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
