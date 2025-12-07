#ifndef UNARCHIVE_H
#define UNARCHIVE_H

#include "archive.h"
#include "file.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool readArchiveHeader(FILE* file, struct ArchiveHeader* header);

bool readFileHeader(FILE* archiveFile, struct FileHeader* header, char** fileName);
bool writeFile(FILE* archiveFile, const char* dirPath);

bool unarchive(const char* filePath);

#endif // UNARCHIVE_H