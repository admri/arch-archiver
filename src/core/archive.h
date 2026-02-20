#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Archive
{
    bool readOnly;
    const char* filePath;
    FILE* file;
    size_t fileCount;
    size_t currentFileIndex;
} Archive;

Archive* createArchive(const char* path, const char* fileMode);
void freeArchive(Archive* archive);

#endif // ARCHIVE_H
