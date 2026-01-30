#ifndef XIPFS_SHARED_API_H
#define XIPFS_SHARED_API_H

#include "xipfs.h"
/**
 * @brief Trampoline functions triggering syscalls that will actually perform the required service.
 */
extern const void *xipfs_safe_exec_syscalls_wrappers[XIPFS_SYSCALL_MAX];

#endif
