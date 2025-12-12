/* 
 * Archive is an archiver program which aims to make archiving files simple.
 * Copyright (C) 2025 Adam Riha
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 * For more information regarding this project please contact me at rihaadam1<at>seznam.cz.
 */

#include "archive.h"
#include "file.h"
#include "unarchive.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: arch [archive_name] [file1] [file2]...\n");
        return 1;
    }

    const char* archiveFilePath = argv[1];

    if (argc > 2)
    {
        struct Archive archive = {
            .filePath = archiveFilePath,
            .fileList = &argv[2],
            .fileCount = argc - 2
        };

        struct ArchiveHeader header = {
            .magic = "ARCH",
            .version = 1,
            .fileCount = 0,
            .reserved = {0}
        };

        if (!createArchive(&archive, &header))
        {
            perror("Failed to create archive");
            return 1;
        }
        
        for (size_t i = 0; i < archive.fileCount; i++)
        {
            if (addToArchive(archive.file, archive.fileList[i]))
            {
                header.fileCount++;
                fseek(archive.file, 4 + sizeof(uint16_t), SEEK_SET);
                if (!writeFile(archive.file, (const char*)&header.fileCount, sizeof(header.fileCount)))
                {
                    perror("Failed to update file count in header");
                    return 2;
                }
                fseek(archive.file, 0, SEEK_END);
            }
            else
            {
                printf("Failed to add file %zu to archive\n", i + 1);
            }
        }
        
        fclose(archive.file);
    }
    else
    {
        if (!unarchive(archiveFilePath))
        {
            printf("Failed to unarchive %s", archiveFilePath);
        }
    }

    return 0;
}
