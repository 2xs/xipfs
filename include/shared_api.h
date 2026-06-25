#ifndef XIPFS_SHARED_API_H
#define XIPFS_SHARED_API_H

#include "xipfs.h"

#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT

#define XIPFS_SHARED_API_CODE_ALIGNMENT 4096
#define XIPFS_SHARED_API_CODE_SIZE      4096

/**
 * @brief Trampoline functions triggering syscalls that will actually perform the required service.
 */
extern const void *xipfs_safe_exec_syscalls_wrappers[XIPFS_SYSCALL_MAX];

typedef struct {
    char *str;
    size_t size;
    const char *format;
    va_list ap;
} xipfs_syscall_vsnprintf_params_t;

#endif

#endif
