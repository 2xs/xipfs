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

/*
 * Fields: {} default, {:x}/{:X} lower/upper hex, {:o} octal, {:p} address.
 * A width pads with spaces; a leading zero pads with zeroes.
 */
typedef struct xipfs_format_spec_s {
    int zero_pad;          /* Use zeroes instead of spaces. */
    unsigned int width;    /* Minimum output width. */
    char conversion;       /* Conversion code; 0 uses the default. */
    size_t end;            /* Index of the closing brace. */
} xipfs_format_spec_t;

static int is_conversion(char c)
{
    return c == 'x' || c == 'X' || c == 'o' || c == 'p';
}

static int parse_spec(const char *fmt, size_t fmt_len, size_t start,
                      xipfs_format_spec_t *spec)
{
    size_t pos = start;

    spec->zero_pad = 0;
    spec->width = 0;
    spec->conversion = 0;

    if (pos >= fmt_len) {
        return -1;
    }
    if (fmt[pos] == '}') {
        spec->end = pos;
        return 0;
    }
    if (fmt[pos] != ':') {
        return -1;
    }
    pos++;

    if (pos < fmt_len && fmt[pos] == '0') {
        spec->zero_pad = 1;
        pos++;
    }
    while (pos < fmt_len && fmt[pos] >= '0' && fmt[pos] <= '9') {
        spec->width = spec->width * 10U + (unsigned int)(fmt[pos] - '0');
        if (spec->width > 99U) {
            return -1;
        }
        pos++;
    }

    if (pos < fmt_len && fmt[pos] != '}') {
        if (!is_conversion(fmt[pos])) {
            return -1;
        }
        spec->conversion = fmt[pos];
        pos++;
    }
    if (pos >= fmt_len || fmt[pos] != '}') {
        return -1;
    }

    spec->end = pos;
    return 0;
}

static void print_format_error(void)
{
    fputs("{?}", stdout);
}

static void print_default(const xipfs_print_arg_t *arg)
{
    switch (arg->tag) {
    case XIPFS_PRINT_ARG_INT:
        printf("%ld", (long)arg->v.i);
        break;
    case XIPFS_PRINT_ARG_UINT:
        printf("%lu", (unsigned long)arg->v.u);
        break;
    case XIPFS_PRINT_ARG_STR:
        printf("%.*s", (int)arg->v.s.len, arg->v.s.p);
        break;
    case XIPFS_PRINT_ARG_HEX:
        printf("0x%lx", (unsigned long)arg->v.h);
        break;
    case XIPFS_PRINT_ARG_CHAR:
        putchar((int)(arg->v.u & 0xffU));
        break;
    default:
        print_format_error();
        break;
    }
}

static char numeric_conversion(const xipfs_print_arg_t *arg,
                               const xipfs_format_spec_t *spec)
{
    if (spec->conversion == 'x' || spec->conversion == 'X'
        || spec->conversion == 'o') {
        return spec->conversion;
    }
    return arg->tag == XIPFS_PRINT_ARG_INT ? 'd' : 'u';
}

static void print_spec(const xipfs_print_arg_t *arg,
                       const xipfs_format_spec_t *spec)
{
    char format[12];
    size_t pos = 0;
    char conversion;

    if (spec->conversion == 0 && spec->width == 0 && !spec->zero_pad) {
        print_default(arg);
        return;
    }
    if (arg->tag == XIPFS_PRINT_ARG_STR) {
        print_default(arg);
        return;
    }
    if (spec->conversion == 'p') {
        printf("0x%08lx", (unsigned long)arg->v.u);
        return;
    }

    format[pos++] = '%';
    if (spec->zero_pad) {
        format[pos++] = '0';
    }
    if (spec->width >= 10U) {
        format[pos++] = (char)('0' + spec->width / 10U);
    }
    if (spec->width > 0U) {
        format[pos++] = (char)('0' + spec->width % 10U);
    }
    format[pos++] = 'l';
    conversion = numeric_conversion(arg, spec);
    format[pos++] = conversion;
    format[pos] = '\0';

    /* The format is assembled from the validated spec above. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    if (conversion == 'd') {
        printf(format, (long)arg->v.i);
    } else {
        printf(format, (unsigned long)arg->v.u);
    }
#pragma GCC diagnostic pop
}

void xipfs_sys_print_fmt(const char *fmt, size_t fmt_len,
                         const xipfs_print_arg_t *args, size_t nargs)
{
    size_t arg_index = 0;

    if (fmt == NULL || (nargs > 0 && args == NULL)) {
        print_format_error();
        return;
    }

    for (size_t i = 0; i < fmt_len; i++) {
        char c = fmt[i];

        if (c == '{') {
            xipfs_format_spec_t spec;

            if (i + 1 < fmt_len && fmt[i + 1] == '{') {
                putchar('{');
                i++;
                continue;
            }
            if (parse_spec(fmt, fmt_len, i + 1, &spec) < 0) {
                print_format_error();
                return;
            }

            i = spec.end;
            if (arg_index < nargs) {
                print_spec(&args[arg_index], &spec);
                arg_index++;
            } else {
                print_format_error();
            }
            continue;
        }

        if (c == '}') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                putchar('}');
                i++;
                continue;
            }
            print_format_error();
            return;
        }

        putchar(c);
    }
}
