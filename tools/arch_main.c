#include <arch/archiver.h>
#include <arch/unarchiver.h>
#include <arch/arch_errors.h>

#include "../src/util/file.h"

#include <locale.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");

    if (argc < 2)
    {
        printf("Usage: %s [archive_name] [file1] [file2]...\n", argv[0]);
        return 1;
    }

    const char* archiveFilePath = argv[1];
    Archive* archive = NULL;

    if (argc > 2)
    {
        size_t fileCount = argc - 2;
        const char** filePaths = (const char**)&argv[2];
        
        ArchResult r = arch_create(archiveFilePath, &archive);
        if (r != ARCH_OK)
        {
            fprintf(stderr, "arch: Failed to create archive: %s\n", arch_strerror(r));
            return 1;
        }

        for (size_t i = 0; i < fileCount; ++i)
        {
            const char* currentPath = filePaths[i];
            printf("Adding '%s' to archive...\n", currentPath);
            
            if (isDirectory(currentPath))
            {
                printf("'%s' is a directory, adding recursively...\n", currentPath);
                r = arch_addDirectory(archive, currentPath);
                if (r != ARCH_OK)
                {
                    fprintf(stderr, "arch: Failed to add directory '%s': %s\n", currentPath, arch_strerror(r));
                }
            }
            else
            {
                printf("'%s' is a file, adding...\n", currentPath);
                r = arch_addFile(archive, currentPath);
                if (r != ARCH_OK)
                {
                    fprintf(stderr, "arch: Failed to add file '%s': %s\n", currentPath, arch_strerror(r));
                }
            }
        }
    }
    else
    {
        ArchResult r = arch_open(archiveFilePath, &archive);
        if (r != ARCH_OK)
        {
            fprintf(stderr, "arch: Failed to open archive: %s\n", arch_strerror(r));
            return 1;
        }

        const char* outputDir = getFileName(archiveFilePath, true);
        if (MKDIR(outputDir) != 0)
        {
            perror("arch: Failed to create output directory");
            arch_close(archive);
            return 1;
        }

        for (size_t i = 1; i <= arch_getFileCount(archive); ++i)
        {
            r = arch_retrieveNextFile(archive, outputDir);
            if (r != ARCH_OK)
            {
                fprintf(stderr, "arch: Failed to extract file #%zu: %s\n", i, arch_strerror(r));
                return 1;
            }
        }
    }

    arch_close(archive);

    return 0;
}
