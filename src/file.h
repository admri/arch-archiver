#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 4096
#endif

#define COMPRESSED_FLAG 1

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0755)
#endif

#ifdef _WIN32
    #define DIR_SEP '\\'
#else
    #define DIR_SEP '/'
#endif

bool readFile(FILE* file, char* buffer, size_t buffer_size, size_t* bytesRead);
bool writeFile(FILE* file, const char* buffer, size_t bytes);
bool copyFileData(FILE* in, FILE* out, uint64_t fileSize);
char* getFileName(const char* filePath, bool stripExtension);

bool compressFileStream(FILE* inFile, FILE* outFile, uint64_t* compSize);
bool decompressFileStream(FILE* inFile, FILE* outFile, uint64_t compSize);

#endif // FILE_H