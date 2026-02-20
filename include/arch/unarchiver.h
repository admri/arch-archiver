#ifndef UNARCHIVER_H
#define UNARCHIVER_H

#include <arch/arch_errors.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Archive Archive;

ArchResult arch_open(const char* path, Archive** outArchive);
ArchResult arch_retrieveNextFile(Archive* archive, const char* output_dir);
void arch_close(Archive* archive);

size_t arch_getFileCount(Archive* archive);

#ifdef __cplusplus
}
#endif

#endif // UNARCHIVER_H
