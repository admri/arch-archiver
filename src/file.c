#include "file.h"

#include <zlib.h>

#include <stdio.h>

bool readFile(FILE* file, char* buffer, size_t buffer_size, size_t* bytesRead)
{
    if (!file || !buffer || !bytesRead) return false;

    clearerr(file);
    *bytesRead = fread(buffer, 1, buffer_size, file);

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

bool copyFileData(FILE* in, FILE* out, uint64_t fileSize)
{
    if (!in || !out) return false;

    unsigned char buffer[BUFFER_SIZE];
    uint64_t bytesLeft = fileSize;

    while (bytesLeft > 0)
    {
        size_t chunk = (bytesLeft < BUFFER_SIZE) ? (size_t)bytesLeft : BUFFER_SIZE;
        size_t readBytes;
        if (!readFile(in, (char*)buffer, chunk, &readBytes))
        {
            return false;
        }
        if (readBytes == 0)
        {
            fprintf(stderr, "Unexpected EOF while copying file data\n");
            return false;
        }
        if (!writeFile(out, (const char*)buffer, readBytes))
        {
            return false;
        }
        bytesLeft -= readBytes;
    }
    return true;
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

bool compressFileStream(FILE* inFile, FILE* outFile, uint64_t* outCompSize)
{
    if (!inFile || !outFile || !outCompSize) return false;

    unsigned char inBuf[BUFFER_SIZE];
    unsigned char outBuf[BUFFER_SIZE];
    uint64_t totalWritten = 0;

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

bool decompressFileStream(FILE* inFile, FILE* outFile, uint64_t compSize)
{
    if (!inFile || !outFile) return false;

    unsigned char inBuf[BUFFER_SIZE];
    unsigned char outBuf[BUFFER_SIZE];

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
