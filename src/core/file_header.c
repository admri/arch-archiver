#include "file_header.h"
#include "../util/file.h"

#include <zlib.h>

#include <stdlib.h>

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

bool createFileHeader(const char* path, uint8_t flags, FileHeader* header, FILE** outFile)
{
    if (!header || !path) return false;

    FILE* file = NULL;
    char* fileName = NULL;

    file = fopen(path, "rb");
    if (!file) return false;

    uint64_t origSize = getFileSize(file);

    fileName = getFileName(path, false);
    if (!fileName)
    {
        fclose(file);
        return false;
    }

    size_t nameLen = strlen(fileName);
    if (nameLen > UINT16_MAX) goto cleanup;

    header->nameLength = (uint16_t)nameLen;
    header->origSize = origSize;
    header->compSize = 0;
    header->flags = flags;

    free(fileName);
    *outFile = file;
    return true;

cleanup:
    if (file) fclose(file);
    free(fileName);
    return false;
}

void freeFileHeader(FileHeader *header)
{
    if (header) free(header);
}

bool writeFileHeader(FILE *file, const FileHeader* header, const char* fileName, uint64_t* compSizePos)
{
    if (!writeFile(file, (const char*)&header->nameLength, sizeof(header->nameLength))) return false;
    if (!writeFile(file, (const char*)&header->origSize, sizeof(header->origSize))) return false;

    int64_t pos = ftell64(file);
    if (pos < 0)
    {
        perror("ftell failed");
        return false;
    }
    *compSizePos = (uint64_t)pos;

    uint64_t zero = 0;
    if (!writeFile(file, (const char*)&zero, sizeof(zero))) return false;
    if (!writeFile(file, (const char*)&header->flags, sizeof(header->flags))) return false;
    if (!writeFile(file, fileName, header->nameLength)) return false;

    return true;
}

bool updateFileHeaderCompSize(FileHeader* header, FILE *file, uint64_t compSizePos, uint64_t compSize)
{
    int64_t origPos = ftell64(file);
    if (origPos < 0) return false;

    if (fseek64(file, (int64_t)compSizePos, SEEK_SET) != 0) return false;
    if (!writeFile(file, (const char*)&compSize, sizeof(compSize))) return false;
    fflush(file);

    if (fseek64(file, origPos, SEEK_SET) != 0) return false;

    header->compSize = compSize;
    return true;
}

bool readFileHeader(FILE* archiveFile, FileHeader* header, char** fileName)
{
    size_t read;

    // Name length
    unsigned char nameLength[sizeof header->nameLength];
    if (!readFile(archiveFile, nameLength, sizeof nameLength, &read) || read != sizeof nameLength)
    {
        perror("Failed to read file name length");
        return false;
    }
    header->nameLength = read_u16_le(nameLength);

    // Original file size
    unsigned char origSize[sizeof header->origSize];
    if (!readFile(archiveFile, origSize, sizeof origSize, &read) || read != sizeof origSize)
    {
        perror("Failed to read file size");
        return false;
    }
    header->origSize = read_u64_le(origSize);

    // Compressed file size
    unsigned char compSize[sizeof header->compSize];
    if (!readFile(archiveFile, compSize, sizeof compSize, &read) || read != sizeof compSize)
    {
        perror("Failed to read compressed file size");
        return false;
    }
    header->compSize = read_u64_le(compSize);

    // Flags
    unsigned char flags[sizeof header->flags];
    if (!readFile(archiveFile, flags, sizeof flags, &read) || read != sizeof flags)
    {
        perror("Failed to read file flags");
        return false;
    }
    header->flags = flags[0];

    // File name
    *fileName = malloc((size_t)header->nameLength + 1);
    if (!*fileName)
    {
        perror("malloc failed");
        return false;
    }
    if (!readFile(archiveFile, *fileName, header->nameLength, &read) || read != header->nameLength)
    {
        perror("Failed to read file name");
        free(*fileName);
        return false;
    }
    (*fileName)[header->nameLength] = '\0';

    return true;
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
