#ifndef ARCHIVER_H
#define ARCHIVER_H

#include <arch/arch_errors.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Archive Archive;

ArchResult arch_create(const char* path, Archive** outArchive);
ArchResult arch_addFile(Archive* archive, const char* path);
ArchResult arch_addDirectory(Archive* archive, const char* path);
void arch_close(Archive* archive);

#ifdef __cplusplus
}
#endif

#endif // ARCHIVER_H
