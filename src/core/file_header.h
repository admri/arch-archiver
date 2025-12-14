#ifndef FILE_HEADER_H
#define FILE_HEADER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define COMPRESSED_FLAG 1

typedef struct FileHeader
{
    uint16_t nameLength;
    uint64_t origSize;
    uint64_t compSize;
    uint8_t flags;
} FileHeader; // 19 bytes (+ variable-sized file name)

bool createFileHeader(const char* path, uint8_t flags, FileHeader* header, FILE** outFile);
void freeFileHeader(FileHeader* header);

bool writeFileHeader(FILE* file, const FileHeader* header, const char* fileName, uint64_t* compSizePos);
bool updateFileHeaderCompSize(FileHeader* header, FILE* file, uint64_t compSizePos, uint64_t compSize);
bool readFileHeader(FILE* archiveFile, FileHeader* header, char** fileName);

bool compressFileStream(FILE* inFile, FILE* outFile, uint64_t* compSize);
bool decompressFileStream(FILE* inFile, FILE* outFile, uint64_t compSize);

#endif // FILE_HEADER_H
