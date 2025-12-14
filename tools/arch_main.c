#include <arch/archiver.h>
#include <arch/unarchiver.h>

#include "../src/util/file.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: %s [archive_name] [file1] [file2]...\n", argv[0]);
        return 1;
    }

    const char* archiveFilePath = argv[1];
    Archive* archive = NULL;

    if (argc > 2)
    {
        int fileCount = argc - 2;
        const char** filePaths = (const char**)&argv[2];
        
        printf("Creating archive: %s\n", archiveFilePath);
        archive = arch_create(archiveFilePath);
        if (!archive)
        {
            printf("Failed to create archive: %s\n", archiveFilePath);
            return 1;
        }

        for (int i = 0; i < fileCount; ++i)
        {
            printf("Adding file to archive: %s\n", filePaths[i]);
            if (!arch_addFile(archive, filePaths[i]))
            {
                printf("Failed to add file to archive: %s\n", filePaths[i]);
            }
        }
    }
    else
    {
        archive = arch_open(archiveFilePath);
        if (!archive)
        {
            printf("Failed to open archive: %s\n", archiveFilePath);
            return 1;
        }

        const char* outputDir = getFileName(archiveFilePath, true);
        if (MKDIR(outputDir) != 0)
        {
            perror("Failed to create output directory");
            arch_close(archive);
            return 1;
        }

        printf("Extracting files from archive: %s\n", archiveFilePath);
        for (size_t i = 1; i <= arch_getFileCount(archive); ++i)
        {
            printf("Extracting file %zu to directory: %s\n", i, outputDir);
            if (!arch_retrieveNextFile(archive, outputDir))
            {
                printf("Failed to extract file %zu from archive\n", i);
            }
        }
    }

    arch_close(archive);

    return 0;
}