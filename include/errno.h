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

#ifndef XIPFS_ERRNO_H
#define XIPFS_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A enumration of all xipfs error numbers
 */
enum xipfs_errno_e {
    /**
     * No error
     */
    XIPFS_OK,
    /**
     * Path is null
     */
    XIPFS_ENULLP,
    /**
     * Path is empty
     */
    XIPFS_EEMPTY,
    /**
     * Invalid character
     */
    XIPFS_EINVAL,
    /**
     * Path is not null-terminated
     */
    XIPFS_ENULTER,
    /**
     * File pointer is null
     */
    XIPFS_ENULLF,
    /**
     * File is not page-aligned
     */
    XIPFS_EALIGN,
    /**
     * File is outside NVM space
     */
    XIPFS_EOUTNVM,
    /**
     * File improperly linked to others
     */
    XIPFS_ELINK,
    /**
     * Offset exceeds max position
     */
    XIPFS_EMAXOFF,
    /**
     * NVMC error
     */
    XIPFS_ENVMC,
    /**
     * Mount point is null
     */
    XIPFS_ENULLM,
    /**
     * Bad magic number
     */
    XIPFS_EMAGIC,
    /**
     * Bad page number
     */
    XIPFS_EPAGNUM,
    /**
     * File system full
     */
    XIPFS_EFULL,
    /**
     * File already exists
     */
    XIPFS_EEXIST,
    /**
     * File has wrong permissions
     */
    XIPFS_EPERM,
    /**
     * Insufficient space to create the file
     */
    XIPFS_ENOSPACE,

#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT

    /**
     * Failed to set text MPU region
     */
    XIPFS_ETEXTREGION,

    /**
     * Failed to set data MPU region
     */
    XIPFS_EDATAREGION,

    /**
     * Failed to set stack MPU region
     */
    XIPFS_ESTACKREGION,

    /**
     * Failed to enable MPU
     */
    XIPFS_EENABLEMPU,

    /**
     * Failed to disable MPU
     */
    XIPFS_EDISABLEMPU,

#else
    /**
     * Files were compiled with no XIPFS_ENABLE_SAFE_EXEC_SUPPORT defined.
     */
    XIPFS_ENOSAFESUPPORT,
#endif /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */

    /**
     * Error number - must be the last element
     */
    XIPFS_ENUM,
};

extern int xipfs_errno;

const char *xipfs_strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_ERRNO_H */
