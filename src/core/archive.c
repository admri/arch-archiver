#include "archive.h"

#include <stdlib.h>
#include <string.h>

Archive* createArchive(const char* path, const char* fileMode)
{
    if (!path || !fileMode) return NULL;

    Archive* archive = malloc(sizeof *archive);
    if (!archive) return NULL;

    archive->readOnly = fileMode[0] == "r";

    archive->filePath = strdup(path);
    if (!archive->filePath)
    {
        free(archive);
        return NULL;
    }

    archive->file = fopen(path, fileMode);
    if (!archive->file)
    {
        free((char*)archive->filePath);
        free(archive);
        return NULL;
    }

    archive->fileCount = 0;
    archive->currentFileIndex = 0;

    return archive;
}

void freeArchive(Archive *archive)
{
    if (!archive) return;

    if (archive->file)
    {
        fflush(archive->file);
        fclose(archive->file);
    }
    if (archive->filePath)
    {
        free((char*)archive->filePath);
    }
    free(archive);
}
