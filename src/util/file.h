#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 4096
#endif

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)

    #define fseek64 _fseeki64
    #define ftell64 _ftelli64
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0755)
    
    #define fseek64 fseeko
    #define ftell64 ftello
#endif

#ifdef _WIN32
    #define DIR_SEP '\\'
#else
    #define DIR_SEP '/'
#endif

uint16_t read_u16_le(const unsigned char b[2]);
uint32_t read_u32_le(const unsigned char b[4]);
uint64_t read_u64_le(const unsigned char b[8]);

bool readFile(FILE* file, char* buffer, size_t buffer_size, size_t* outbytesRead);
bool writeFile(FILE* file, const char* buffer, size_t bytes);
bool copyFileData(FILE* in, FILE* out, uint64_t fileSize, uint32_t* outCrc);

uint64_t getFileSize(FILE* file);
char* getFileName(const char* filePath, bool stripExtension);

bool compressFileStream(FILE* inFile, FILE* outFile, uint64_t* outCompSize, uint32_t* outCrcUncompressed, uint32_t* outCrcCompressed);
bool decompressFileStream(FILE* inFile, FILE* outFile, uint64_t compSize, uint32_t* outCrcUncompressed, uint32_t* outCrcCompressed);

#endif // FILE_H