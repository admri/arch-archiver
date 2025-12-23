#include "file.h"

#include <zlib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

uint16_t read_u16_le(const unsigned char b[2])
{
    return ((uint16_t)b[0] |
           ((uint16_t)b[1] << 8));
}

uint32_t read_u32_le(const unsigned char b[4])
{
    return ((uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24));
}

uint64_t read_u64_le(const unsigned char b[8])
{
    uint64_t res = 0;
    for (int i = 0; i < 8; i++)
    {
        res |= (uint64_t)b[i] << (8 * i);
    }
    return res;
}

bool readFile(FILE* file, char* buffer, size_t buffer_size, size_t* outBytesRead)
{
    if (!file || !buffer || !outBytesRead) return false;

    clearerr(file);
    *outBytesRead = fread(buffer, 1, buffer_size, file);

    if (ferror(file))
    {
        perror("Error reading file");
        return false;
    }

    return true;
}

bool writeFile(FILE* file, const char* buffer, size_t bytes)
{
    if (!file || !buffer) return false;

    size_t written = fwrite(buffer, 1, bytes, file);
    if (written != bytes)
    {
        perror("Error writing file");
        return false;
    }

    return true;
}

bool copyFileData(FILE* in, FILE* out, uint64_t fileSize, uint32_t* outCrc)
{
    if (!in || !out) return false;

    unsigned char buffer[BUFFER_SIZE];
    uint64_t bytesLeft = fileSize;
    *outCrc = crc32(0L, Z_NULL, 0);

    while (bytesLeft > 0)
    {
        // Read chunk (max BUFFER_SIZE)
        size_t chunk = (bytesLeft < BUFFER_SIZE) ? (size_t)bytesLeft : BUFFER_SIZE;
        size_t readBytes;
        if (!readFile(in, (char*)buffer, chunk, &readBytes)) return false;
        if (readBytes == 0) return false;

        // Update CRC
        *outCrc = crc32(*outCrc, buffer, (uInt)readBytes);
        
        // Write chunk
        if (!writeFile(out, (const char*)buffer, readBytes)) return false;
        
        bytesLeft -= readBytes;
    }

    return true;
}

uint64_t getFileSize(FILE* file)
{
    if (!file) return 0;

    int64_t currentPos = ftell64(file);
    if (currentPos < 0) return 0;

    if (fseek64(file, 0, SEEK_END) != 0) return 0;

    int64_t size = ftell64(file);
    if (size < 0) return 0;

    if (fseek64(file, currentPos, SEEK_SET) != 0) return 0;

    return (uint64_t)size;
}

char* getFileName(const char* filePath, bool stripExtension)
{
    const char* separator = strrchr(filePath, DIR_SEP);
    const char* last = separator ? separator + 1 : filePath;

    size_t len = strlen(last);
    char* fileName = malloc(len + 1);
    if (!fileName) return NULL;

    strcpy(fileName, last);

    if (stripExtension)
    {
        char* dot = strrchr(fileName, '.');
        if (dot) *dot = '\0';
    }

    return fileName;
}

bool compressFileStream(FILE* inFile, FILE* outFile, uint64_t* outCompSize, uint32_t* outCrcUncompressed, uint32_t* outCrcCompressed)
{
    if (!inFile || !outFile || !outCompSize || !outCrcUncompressed || !outCrcCompressed) return false;

    unsigned char inBuf[BUFFER_SIZE];
    unsigned char outBuf[BUFFER_SIZE];
    uint64_t totalWritten = 0;

    *outCrcUncompressed = 0;
    *outCrcCompressed = 0;

    z_stream strm = {0};

    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        fprintf(stderr, "deflateInit failed\n");
        return false;
    }

    int flush;
    do
    {
        size_t readBytes;
        if (!readFile(inFile, (char*)inBuf, BUFFER_SIZE, &readBytes))
        {
            deflateEnd(&strm);
            return false;
        }

        if (readBytes > 0)
        {
            *outCrcUncompressed = crc32(*outCrcUncompressed, inBuf, (uInt)readBytes);
        }

        flush = feof(inFile) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = inBuf;
        strm.avail_in = (uInt)readBytes;

        do
        {
            strm.next_out = outBuf;
            strm.avail_out = BUFFER_SIZE;

            int ret = deflate(&strm, flush);
            if (ret == Z_STREAM_ERROR)
            {
                deflateEnd(&strm);
                fprintf(stderr, "deflate error: Z_STREAM_ERROR\n");
                return false;
            }

            size_t have = BUFFER_SIZE - strm.avail_out;
            if (have > 0)
            {
                *outCrcCompressed = crc32(*outCrcCompressed, outBuf, (uInt)have);

                if (!writeFile(outFile, (const char*)outBuf, have))
                {
                    deflateEnd(&strm);
                    return false;
                }

                totalWritten += have;
            }
        } while (strm.avail_out == 0);
    } while (flush != Z_FINISH || strm.avail_in > 0);

    deflateEnd(&strm);

    *outCompSize = totalWritten;
    return true;
}

bool decompressFileStream(FILE* inFile, FILE* outFile, uint64_t compSize, uint32_t* outCrcUncompressed, uint32_t* outCrcCompressed)
{
    if (!inFile || !outFile) return false;

    unsigned char inBuf[BUFFER_SIZE];
    unsigned char outBuf[BUFFER_SIZE];
    
    *outCrcUncompressed = 0;
    *outCrcCompressed = 0;

    z_stream strm = {0};
    
    if (inflateInit(&strm) != Z_OK)
    {
        fprintf(stderr, "inflateInit failed\n");
        return false;
    }

    uint64_t totalRead = 0;
    int ret = Z_OK;

    while (ret != Z_STREAM_END && totalRead < compSize)
    {
        size_t toRead = (compSize - totalRead < BUFFER_SIZE)
                        ? (size_t)(compSize - totalRead)
                        : BUFFER_SIZE;

        size_t bytesRead;
        if (!readFile(inFile, (char*)inBuf, toRead, &bytesRead) || (bytesRead == 0 && !feof(inFile)))
        {
            inflateEnd(&strm);
            return false;
        }

        if (bytesRead > 0)
        {
            *outCrcCompressed = crc32(*outCrcCompressed, inBuf, (uInt)bytesRead);
        }

        totalRead += bytesRead;

        strm.next_in = inBuf;
        strm.avail_in = (uInt)bytesRead;

        while (strm.avail_in > 0)
        {
            strm.next_out = outBuf;
            strm.avail_out = BUFFER_SIZE;

            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                inflateEnd(&strm);
                fprintf(stderr, "inflate error: %d\n", ret);
                return false;
            }

            size_t have = BUFFER_SIZE - strm.avail_out;
            if (have > 0)
            {
                *outCrcUncompressed = crc32(*outCrcUncompressed, outBuf, (uInt)have);

                if (!writeFile(outFile, (const char*)outBuf, have))
                {
                    inflateEnd(&strm);
                    return false;
                }
            }
        }
    }

    inflateEnd(&strm);
    return ret == Z_STREAM_END;
}
