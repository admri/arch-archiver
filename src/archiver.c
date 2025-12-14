#include <arch/archiver.h>

#include "core/archive.h"
#include "core/archive_header.h"
#include "core/file_header.h"
#include "util/file.h"

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
    if (!createFileHeader(path, COMPRESSED_FLAG, &fileHeader, &file)) return false;

    fileName = getFileName(path, false);
    if (!fileName) goto cleanup;

    uint64_t compSizePos = 0;
    if (!writeFileHeader(archive->file, &fileHeader, fileName, &compSizePos)) goto cleanup;

    uint64_t compSize = 0;
    if (!compressFileStream(file, archive->file, &compSize)) goto cleanup;

    if (!updateFileHeaderCompSize(&fileHeader, archive->file, compSizePos, compSize)) goto cleanup;

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
