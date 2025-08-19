/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2024)                */
/*  Copyright (C) 2020-2024 Orange                                             */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a minimal,     */
/*  hypervisor relying on proven properties such as memory isolation.          */
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
 * libc include
 */
#include <errno.h>

/*
 * xipfs include
 */
#include "include/errno.h"

/*
 * Global variables
 */

/**
 * @brief The xipfs errno global variable
 */
int xipfs_errno = XIPFS_OK;

/**
 * @internal
 *
 * @brief Map xipfs errno number to the associated error string
 */
static const char *xipfs_errno_to_str[XIPFS_ENUM] = {
    [XIPFS_OK] = "",
    [XIPFS_ENULLP] = "path is null",
    [XIPFS_EEMPTY] = "path is empty",
    [XIPFS_EINVAL] = "invalid character",
    [XIPFS_ENULTER] = "path is not null-terminated",
    [XIPFS_ENULLF] = "file pointer is null",
    [XIPFS_EALIGN] = "file is not page-aligned",
    [XIPFS_EOUTNVM] = "file is outside NVM space",
    [XIPFS_ELINK] = "file improperly linked to others",
    [XIPFS_EMAXOFF] = "offset exceeds max position",
    [XIPFS_ENVMC] = "NVMC error",
    [XIPFS_ENULLM] = "mount point is null",
    [XIPFS_EMAGIC] = "bad magic number",
    [XIPFS_EPAGNUM] = "bad page number",
    [XIPFS_EFULL] = "file system full",
    [XIPFS_EEXIST] = "file already exists",
    [XIPFS_EPERM] = "file has wrong permissions",
    [XIPFS_ENOSPACE] = "insufficient space to create the file",
#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT
    [XIPFS_ETEXTREGION] = "failed to set text MPU region",
    [XIPFS_EDATAREGION] = "failed to set data MPU region",
    [XIPFS_ESTACKREGION] = "failed to set stack MPU region",
    [XIPFS_EENABLEMPU] = "failed to enable MPU",
    [XIPFS_EDISABLEMPU] = "failed to enable MPU",
#else
    [XIPFS_ENOSAFESUPPORT] = "no safe execution support"
#endif /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */

};

/**
 * @brief Maps xipfs errno number to the associated error string
 *
 * @param errnum The errno number to map
 *
 * @return The associated error string
 */
const char *
xipfs_strerror(int errnum)
{
    unsigned num = errnum;

    if (num >= XIPFS_ENUM) {
        return "unknown xipfs errno";
    }

    return xipfs_errno_to_str[errnum];
}
