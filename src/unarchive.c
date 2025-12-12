#include "unarchive.h"

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

bool readArchiveHeader(FILE* file, struct ArchiveHeader* header)
{
    size_t read;

    // Magic number
    unsigned char magic[sizeof header->magic];

    readFile(file, magic, sizeof magic, &read);
    if (read != sizeof magic)
    {
        perror("Failed to read magic number");
        return false;
    }

    memcpy(header->magic, magic, sizeof magic);

    // Version number
    unsigned char version[sizeof header->version];

    readFile(file, version, sizeof version, &read);
    if (read != sizeof version)
    {
        perror("Failed to read archive version");
        return false;
    }

    header->version = read_u16_le(version);

    // Files count
    unsigned char fileCount[sizeof header->fileCount];

    readFile(file, fileCount, sizeof fileCount, &read);

    if (read != sizeof fileCount)
    {
        perror("Failed to read files count");
        return false;
    }

    header->fileCount = read_u32_le(fileCount);

    // Reserved bytes
    if (fseek(file, sizeof header->reserved, SEEK_CUR) != 0)
    {
        return false;
    }

    return true;
}

bool readFileHeader(FILE* archiveFile, struct FileHeader* header, char** fileName)
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

bool copyNextFile(FILE* archiveFile, const char* dirPath)
{
    struct FileHeader header;
    char* fileName = NULL;

    if (!readFileHeader(archiveFile, &header, &fileName))
    {
        fprintf(stderr, "Failed to read file header\n");
        return false;
    }

    // Construct file path
    size_t filePathSize = strlen(dirPath) + sizeof(DIR_SEP) + strlen(fileName) + 1;
    char* filePath = malloc(filePathSize);
    if (!filePath)
    {
        perror("malloc failed");
        free(fileName);
        return false;
    }
    snprintf(filePath, filePathSize, "%s%c%s", dirPath, DIR_SEP, fileName);

    FILE* file = fopen(filePath, "wb");
    if (!file)
    {
        perror("Failed to create output file");
        free(filePath);
        free(fileName);
        return false;
    }

    bool ok = true;
    if (header.flags & COMPRESSED_FLAG)
    {
        ok = decompressFileStream(archiveFile, file, header.compSize);
    }
    else
    {
        ok = copyFileData(archiveFile, file, header.origSize);
    }

    if (!ok)
    {
        fprintf(stderr, "Failed handling file %s\n", fileName);
        free(filePath);
        free(fileName);
        fclose(file);
        return false;
    }

    free(filePath);
    free(fileName);
    fclose(file);
    return true;
}

bool unarchive(const char* filePath)
{
    FILE* archiveFile = fopen(filePath, "rb");
    if (!archiveFile)
    {
        perror("Failed to open archive");
        return false;
    }

    char* dirName = getFileName(filePath, true);
    if (!dirName) dirName = "archive";
    if (MKDIR(dirName) != 0)
    {
        perror("Failed to create directory");
        free(dirName);
        fclose(archiveFile);
        return false;
    }

    struct ArchiveHeader header;
    if (!readArchiveHeader(archiveFile, &header))
    {
        fprintf(stderr, "Failed to read archive header\n");
        free(dirName);
        fclose(archiveFile);
        return false;
    }

    for (int i = 0; i < header.fileCount; i++)
    {
        if (!copyNextFile(archiveFile, dirName))
        {
            fprintf(stderr, "Failed to unarchive file %u\n", i + 1);
        }
    }

    free(dirName);
    fclose(archiveFile);
    return true;
}