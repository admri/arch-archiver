#include "file_header.h"
#include "../util/file.h"

#include <stdlib.h>
#include <string.h>

bool createFileHeader(const char* path, uint8_t flags, FileHeader* header, FILE** outFile, uint64_t* outOrigSize)
{
    if (!header || !path) return false;

    FILE* file = NULL;
    char* fileName = NULL;

    file = fopen(path, "rb");
    if (!file) return false;

    *outOrigSize = getFileSize(file);

    fileName = getFileName(path, false);
    if (!fileName)
    {
        fclose(file);
        return false;
    }

    size_t nameLen = strlen(fileName);
    if (nameLen > UINT16_MAX) goto cleanup;

    header->magic = FILE_MAGIC;
    header->nameLength = (uint16_t)nameLen;
    header->origSize = *outOrigSize;
    header->compSize = 0;
    header->crc32_uncompressed = 0;
    header->crc32_compressed = 0;
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

bool writeFileHeader(FILE *file, const FileHeader* header, const char* fileName, uint64_t* outCompSizePos, uint64_t* outCrcUncompressedPos, uint64_t* outCrcCompressedPos)
{
    if (!file || !header || !fileName || !outCompSizePos) return false;

    if (!writeFile(file, (const char*)&header->magic, sizeof(header->magic))) return false;
    if (!writeFile(file, (const char*)&header->nameLength, sizeof(header->nameLength))) return false;
    if (!writeFile(file, (const char*)&header->origSize, sizeof(header->origSize))) return false;

    // Compressed size placeholder
    uint64_t zero_64 = 0;
    int64_t pos = ftell64(file);
    if (pos < 0)
    {
        perror("ftell failed");
        return false;
    }
    *outCompSizePos = (uint64_t)pos;
    if (!writeFile(file, (const char*)&zero_64, sizeof(zero_64))) return false;

    // CRC32 placeholders
    uint32_t zero_32 = 0;
    pos = ftell64(file);
    if (pos < 0)
    {
        perror("ftell failed");
        return false;
    }
    *outCrcUncompressedPos = (uint64_t)pos;
    if (!writeFile(file, (const char*)&zero_32, sizeof(zero_32))) return false;

    pos = ftell64(file);
    if (pos < 0)
    {
        perror("ftell failed");
        return false;
    }
    *outCrcCompressedPos = (uint64_t)pos;
    if (!writeFile(file, (const char*)&zero_32, sizeof(zero_32))) return false;

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

bool updateFileHeaderCRC32(FileHeader *header, FILE *file, uint64_t crc32UncompressedPos, uint64_t crc32CompressedPos, uint32_t crc32Uncompressed, uint32_t crc32Compressed)
{
    int64_t origPos = ftell64(file);
    if (origPos < 0) return false;

    if (fseek64(file, (int64_t)crc32UncompressedPos, SEEK_SET) != 0) return false;
    if (!writeFile(file, (const char*)&crc32Uncompressed, sizeof(crc32Uncompressed))) return false;
    fflush(file);

    if (fseek64(file, (int64_t)crc32CompressedPos, SEEK_SET) != 0) return false;
    if (!writeFile(file, (const char*)&crc32Compressed, sizeof(crc32Compressed))) return false;
    fflush(file);

    if (fseek64(file, origPos, SEEK_SET) != 0) return false;

    header->crc32_uncompressed = crc32Uncompressed;
    header->crc32_compressed = crc32Compressed;
    return true;
}

bool readFileHeader(FILE* archiveFile, FileHeader* header, char** fileName)
{
    size_t read;

    // Magic number
    unsigned char magic[sizeof header->magic];
    if (!readFile(archiveFile, magic, sizeof magic, &read) || read != sizeof magic)
    {
        perror("Failed to read file magic");
        return false;
    }
    header->magic = read_u32_le(magic);

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

    // CRC uncompressed
    unsigned char crcUncompressed[sizeof header->crc32_uncompressed];
    if (!readFile(archiveFile, crcUncompressed, sizeof crcUncompressed, &read) || read != sizeof crcUncompressed)
    {
        perror("Failed to read CRC32 uncompressed");
        return false;
    }
    header->crc32_uncompressed = read_u32_le(crcUncompressed);

    // CRC compressed
    unsigned char crcCompressed[sizeof header->crc32_compressed];
    if (!readFile(archiveFile, crcCompressed, sizeof crcCompressed, &read) || read != sizeof crcCompressed)
    {
        perror("Failed to read CRC32 compressed");
        return false;
    }
    header->crc32_compressed = read_u32_le(crcCompressed);

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
