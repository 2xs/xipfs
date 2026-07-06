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

/*
 * libc includes
 */
#include <stdio.h>

/*
 * xipfs include
 */
#include "include/fmt.h"

/* Default formatting for `{}`, by tag. */
static void print_one(const xipfs_print_arg_t *a)
{
    switch (a->tag) {
    case 0: printf("%ld",   (long)a->v.i);              break;
    case 1: printf("%lu",   (unsigned long)a->v.u);     break;
    case 2: printf("%.*s",  (int)a->v.s.len, a->v.s.p); break;
    case 3: printf("0x%lx", (unsigned long)a->v.h);     break;
    case 4: putchar((int)(a->v.u & 0xff));              break;
    }
}

/* Apply a parsed `{:0Nx}`-style spec by building a printf conversion. */
static void print_spec(const xipfs_print_arg_t *a, int zero, int width, char type)
{
    if (type == 0 && width == 0 && !zero) {
        print_one(a);
        return;
    }
    /* strings ignore numeric specs */
    if (a->tag == 2) {
        printf("%.*s", (int)a->v.s.len, a->v.s.p);
        return;
    }
    if (type == 'p') {
        printf("0x%08lx", (unsigned long)a->v.u);
        return;
    }
    char f[12];
    int k = 0;
    f[k++] = '%';
    if (zero) {
        f[k++] = '0';
    }
    if (width >= 10) {
        f[k++] = (char)('0' + (width / 10) % 10);
    }
    if (width > 0) {
        f[k++] = (char)('0' + width % 10);
    }
    f[k++] = 'l';
    /* `f` is built on purpose; silence -Wformat-nonliteral */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    if (type == 'x' || type == 'X' || type == 'o') {
        f[k++] = type;
        f[k] = 0;
        printf(f, (unsigned long)(a->tag == 0 ? (unsigned long)a->v.i : a->v.u));
    } else if (a->tag == 0) {
        f[k++] = 'd';
        f[k] = 0;
        printf(f, (long)a->v.i);
    } else {
        f[k++] = 'u';
        f[k] = 0;
        printf(f, (unsigned long)a->v.u);
    }
#pragma GCC diagnostic pop
}

/*
 * Walk the format string, substituting `{}`/`{:spec}` with the next
 * argument. `{{`/`}}` are literal braces. No auto-newline.
 */
void xipfs_sys_print_fmt(const char *fmt, size_t fmt_len,
                         const xipfs_print_arg_t *args, size_t nargs)
{
    size_t ai = 0;
    for (size_t i = 0; i < fmt_len; i++) {
        char c = fmt[i];
        if (c == '{') {
            if (i + 1 < fmt_len && fmt[i + 1] == '{') { putchar('{'); i++; continue; }
            size_t j = i + 1;
            if (j < fmt_len && fmt[j] == ':') {
                j++;
            }
            int zero = 0;
            if (j < fmt_len && fmt[j] == '0') { zero = 1; j++; }
            int width = 0;
            while (j < fmt_len && fmt[j] >= '0' && fmt[j] <= '9') {
                width = width * 10 + (fmt[j] - '0');
                j++;
            }
            char type = 0;
            if (j < fmt_len && fmt[j] != '}') {
                type = fmt[j];
                j++;
            }
            while (j < fmt_len && fmt[j] != '}') {
                j++;
            }
            i = j; /* loop ++ steps past '}' */
            if (ai < nargs) {
                print_spec(&args[ai++], zero, width, type);
            } else {
                fputs("{?}", stdout);
            }
            continue;
        }
        if (c == '}' && i + 1 < fmt_len && fmt[i + 1] == '}') { putchar('}'); i++; continue; }
        putchar(c);
    }
}
