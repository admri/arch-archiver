#include "archive.h"

bool writeArchiveHeader(FILE* file, struct ArchiveHeader* header)
{
    if (!writeFile(file, header->magic, sizeof(header->magic))) return false;
    if (!writeFile(file, (const char*)&header->version, sizeof(header->version))) return false;
    if (!writeFile(file, (const char*)&header->fileCount, sizeof(header->fileCount))) return false;
    if (!writeFile(file, header->reserved, sizeof(header->reserved))) return false;
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

    archive->file = file;

    return true;
}

bool writeFileHeader(FILE* file, struct FileHeader* header, const char* fileName, long* compSizePos)
{
    if (!writeFile(file, (const char*)&header->nameLength, sizeof(header->nameLength))) return false;
    if (!writeFile(file, (const char*)&header->origSize, sizeof(header->origSize))) return false;

    long pos = ftell(file);
    if (pos == -1L)
    {
        perror("ftell failed");
        return false;
    }
    *compSizePos = pos;

    uint64_t zero = 0;
    if (!writeFile(file, (const char*)&zero, sizeof(zero))) return false;
    if (!writeFile(file, (const char*)&header->flags, sizeof(header->flags))) return false;
    if (!writeFile(file, fileName, header->nameLength)) return false;

    return true;
}

bool addToArchive(FILE* archiveFile, const char* filePath)
{
    if (!archiveFile || !filePath) return false;

    FILE* in = fopen(filePath, "rb");
    if (!in)
    {
        perror("Failed to open input file");
        return false;
    }
    
    if (fseek(in, 0, SEEK_END) != 0)
    {
        perror("ftell fail");
        fclose(in);
        return false;
    }
    long tell = ftell(in);
    if (tell == -1L)
    {
        perror("ftell fail");
        fclose(in);
        return false;
    }
    uint64_t origSize = (uint64_t)tell;
    fseek(in, 0, SEEK_SET);

    char* fileName = getFileName(filePath, false);
    if (!fileName)
    {
        perror("Failed to get file name");
        fclose(in);
        return false;
    }
    size_t nameLen = strlen(fileName);
    if (nameLen > UINT16_MAX)
    {
        fprintf(stderr, "file name too long\n");
        free(fileName); fclose(in);
        return false;
    }

    struct FileHeader header = {
        .nameLength = (uint16_t)nameLen,
        .origSize = origSize,
        .compSize = 0,
        .flags = COMPRESSED_FLAG
    };

    long compSizePos;
    if (!writeFileHeader(archiveFile, &header, fileName, &compSizePos))
    {
        perror("Failed to write file header");
        free(fileName);
        fclose(in);
        return false;
    }

    free(fileName);

    uint64_t compSize;
    if (!compressFileStream(in, archiveFile, &compSize))
    {
        perror("Failed to compress file data");
        fclose(in);
        return false;
    }

    if (fflush(archiveFile) != 0)
    {
        perror("fflush failed");
        fclose(in);
        return false;
    }
    if (fseek(archiveFile, compSizePos, SEEK_SET) != 0)
    {
        perror("fseek failed");
        fclose(in);
        return false;
    }
    if (!writeFile(archiveFile, (const char*)&compSize, sizeof(uint64_t)))
    {
        perror("Failed to update compressed size in header");
        fclose(in);
        return false;
    }
    fflush(archiveFile);
    if (fseek(archiveFile, 0, SEEK_END) != 0)
    {
        perror("fseek end failed");
        fclose(in);
        return false;
    }

    fclose(in);
    return true;
}
