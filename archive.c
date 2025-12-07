#include "archive.h"

bool writeArchiveHeader(FILE* file, struct ArchiveHeader* header)
{
    if (!writeFile(file, header->magic, sizeof(header->magic)))
        return false;
    if (!writeFile(file, (const char*)&header->version, sizeof(uint16_t)))
        return false;
    if (!writeFile(file, (const char*)&header->fileCount, sizeof(uint32_t)))
        return false;
    if (!writeFile(file, header->reserved, sizeof(header->reserved)))
        return false;

    return true;
}

bool createArchive(struct Archive* archive, struct ArchiveHeader* header)
{
    FILE* file = fopen(archive->filePath, "wb+");
    if (!file)
    {
        perror("Failed to open archive file");
        return false;
    }

    if (!writeArchiveHeader(file, header))
    {
        perror("Failed to write archive header");
        fclose(file);
        return false;
    }

    fseek(file, 0, SEEK_END);
    archive->file = file;

    return true;
}

bool writeFileHeader(struct Archive* archive, const char* filePath, uint64_t fileSize)
{
    char* fileName = getFileName(filePath, false);
    size_t nameLength = strlen(fileName);
    
    struct FileHeader header = {
        .nameLength = (uint16_t)nameLength,
        .size = fileSize
    };

    if (!writeFile(archive->file, (const char*)&header.nameLength, sizeof(uint16_t)))
    {
        free(fileName);
        return false;
    }
    if (!writeFile(archive->file, (const char*)&header.size, sizeof(uint64_t)))
    {
        free(fileName);
        return false;
    }
    if (!writeFile(archive->file, fileName, nameLength))
    {
        free(fileName);
        return false;
    }

    free(fileName);
    return true;
}

bool addToArchive(struct Archive* archive, const char* filePath)
{
    FILE* in = fopen(filePath, "rb");
    if (!in)
    {
        perror("Failed to open input file");
        return false;
    }
    
    fseek(in, 0, SEEK_END);
    uint64_t fileSize = ftell(in);
    fseek(in, 0, SEEK_SET);

    if (!writeFileHeader(archive, filePath, fileSize))
    {
        perror("Failed to write file header");
        fclose(in);
        return false;
    }

    if (!copyFileData(in, archive->file, fileSize))
    {
        perror("Failed to write file data");
        fclose(in);
        return false;
    }

    fclose(in);
    return true;
}