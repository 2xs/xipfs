/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2025)                */
/*  Copyright (C) 2020-2025 Orange                                             */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a filesystem   */
/*  with in-place execution and memory isolation.                              */
/*                                                                             */
/*  This software is governed by the CeCILL license under French law and       */
/*  abiding by the rules of distribution of free software.  You can  use,      */
/*  modify and/ or redistribute the software under the terms of the CeCILL     */
/*  license as circulated by CEA, CNRS and INRIA at the following URL          */
/*  "http://www.cecill.info".                                                  */
/*                                                                             */
/*  As a counterpart to the access to the source code and  rights to copy,     */
/*  modify and redistribute granted by the license, users are provided only    */
/*  with a limited warranty  and the software's author,  the holder of the     */
/*  economic rights,  and the successive licensors  have only  limited         */
/*  liability.                                                                 */
/*                                                                             */
/*  In this respect, the user's attention is drawn to the risks associated     */
/*  with loading,  using,  modifying and/or developing or reproducing the      */
/*  software by the user in light of its specific status of free software,     */
/*  that may mean  that it is complicated to manipulate,  and  that  also      */
/*  therefore means  that it is reserved for developers  and  experienced      */
/*  professionals having in-depth computer knowledge. Users are therefore      */
/*  encouraged to load and test the software's suitability as regards their    */
/*  requirements in conditions enabling the security of their systems and/or   */
/*  data to be ensured and,  more generally, to use and operate it in the      */
/*  same conditions as regards security.                                       */
/*                                                                             */
/*  The fact that you are presently reading this means that you have had       */
/*  knowledge of the CeCILL license and that you accept its terms.             */
/*******************************************************************************/

#ifndef XIPFS_FMT_H
#define XIPFS_FMT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Tags used by rust-xipfs-lib's `PrintArg`. */
typedef enum xipfs_print_arg_tag_e {
    XIPFS_PRINT_ARG_INT  = 0,
    XIPFS_PRINT_ARG_UINT = 1,
    XIPFS_PRINT_ARG_STR  = 2,
    XIPFS_PRINT_ARG_HEX  = 3,
    XIPFS_PRINT_ARG_CHAR = 4,
} xipfs_print_arg_tag_t;

/** Must match rust-xipfs-lib's `#[repr(C, u32)] PrintArg`. */
typedef struct xipfs_print_arg_s {
    uint32_t tag;
    union {
        int32_t  i;
        uint32_t u;
        struct { const char *p; uint32_t len; } s;
        uint32_t h;
    } v;
} xipfs_print_arg_t;

void xipfs_sys_print_fmt(const char *fmt, size_t fmt_len,
                         const xipfs_print_arg_t *args, size_t nargs);

typedef void (*xipfs_syscall_sys_print_fmt_t)(const char *fmt, size_t fmt_len,
                                              const xipfs_print_arg_t *args,
                                              size_t nargs);

/** Packed because SVC carries the syscall number and only three arguments. */
typedef struct xipfs_print_fmt_call_s {
    const char *fmt;
    size_t fmt_len;
    const xipfs_print_arg_t *args;
    size_t nargs;
} xipfs_print_fmt_call_t;

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_FMT_H */
