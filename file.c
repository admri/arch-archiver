#include "file.h"

bool readFile(FILE* file, char* buffer, size_t buffer_size, size_t* bytesRead)
{
    *bytesRead = fread(buffer, 1, buffer_size, file);
    if (*bytesRead == 0)
    {
        return false; // EOF
    }
    if (ferror(file))
    {
        perror("Error reading file");
        return false;
    }
    return true;
}

bool writeFile(FILE* file, const char* buffer, size_t bytes)
{
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
    char buffer[BUFFER_SIZE];
    uint64_t bytesLeft = fileSize;

    while (bytesLeft > 0)
    {
        size_t chunk = BUFFER_SIZE < bytesLeft ? BUFFER_SIZE : (size_t)bytesLeft;

        size_t read;
        if (!readFile(in, buffer, chunk, &read))
        {
            perror("Failed to read file data");
            return false;
        }

        if (!writeFile(out, buffer, read))
        {
            perror("Failed to write file data");
            return false;            
        }

        bytesLeft -= read;
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