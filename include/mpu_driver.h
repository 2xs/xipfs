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

#ifndef XIPFS_MPU_DRIVER_H
#define XIPFS_MPU_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MPU regions identifiers.
 *
 * @remarks These identifiers have been chosen with respect to
 * regions ids used by RIOT OS for stack guard (1) and ram no exec(0).
 *
 * @remarks "Where there is an overlap between two regions, the register with
 * the highest region number takes priority."
 * From https://developer.arm.com/documentation/ddi0403/d/System-Level-Architecture/System-Address-Map/Protected-Memory-System-Architecture--PMSAv7/PMSAv7-compliant-MPU-operation?lang=en
 */
typedef enum xipfs_mpu_region_enum_e {
    XIPFS_MPU_REGION_ENUM_TEXT        = 2,
    XIPFS_MPU_REGION_ENUM_EXTRA_TEXT  = 3,
    XIPFS_MPU_REGION_ENUM_DATA        = 4,
    XIPFS_MPU_REGION_ENUM_EXTRA_DATA  = 5,
    XIPFS_MPU_REGION_ENUM_STACK       = 6,


    XIPFS_MPU_REGION_ENUM_FIRST       = XIPFS_MPU_REGION_ENUM_TEXT,
    XIPFS_MPU_REGION_ENUM_LAST        = XIPFS_MPU_REGION_ENUM_STACK
} xipfs_mpu_region_enum_t;

/* eXecute Never */
typedef enum xipfs_mpu_region_xn_enum_e {
    XIPFS_MPU_REGION_EXC_OK = 0,
    XIPFS_MPU_REGION_EXC_NO = 1,

    XIPFS_MPU_REGION_EXC_FIRST = XIPFS_MPU_REGION_EXC_OK,
    XIPFS_MPU_REGION_EXC_LAST  = XIPFS_MPU_REGION_EXC_NO,
} xipfs_mpu_region_xn_enum_t;

/* Access permission words */
typedef enum xipfs_mpu_region_ap_enum_e {
    XIPFS_MPU_REGION_AP_NO_NO    = 0, /* no access for all levels */
    XIPFS_MPU_REGION_AP_RW_NO    = 1, /* read/write for privileged level, no access from user level */
    XIPFS_MPU_REGION_AP_RW_RO    = 2, /* read/write for privileged level, read-only for user level */
    XIPFS_MPU_REGION_AP_RW_RW    = 3, /* read/write for all levels */
    XIPFS_MPU_REGION_AP_RESERVED = 4,
    XIPFS_MPU_REGION_AP_RO_NO    = 5, /* read-only for privileged level, no access from user level */
    XIPFS_MPU_REGION_AP_RO_RO    = 6, /* read-only for all levels */

    XIPFS_MPU_REGION_AP_FIRST    = XIPFS_MPU_REGION_AP_NO_NO,
    XIPFS_MPU_REGION_AP_LAST     = XIPFS_MPU_REGION_AP_RO_RO,
} xipfs_mpu_region_ap_enum_t;

/**
 * @brief MPU Region configuration.
 *
 * @param[in] mpu_region Region id.
 * @param[in] address Region start address.
 * @param[in] size Region size in bytes.
 * @param[in] xn eXecute Never bit.
 * @param[in] ap Access Permission flags
 *
 * @remarks size must be greater than or equal to 32, an be a power of 2.
 * @remarks address must be a multiple of size.
 * @remarks mpu_region, xn and ap must be valid members of their enumeration type.
 *
 * @retval -1 on invalid parameters, when there is no mpu,
 * or when XIPFS_ENABLE_SAFE_EXEC_SUPPORT has not been defined.
 * @retval 0 on success.
 */
extern int xipfs_mpu_configure_region(
    xipfs_mpu_region_enum_t mpu_region, void *address, uint32_t size,
    xipfs_mpu_region_xn_enum_t xn, xipfs_mpu_region_ap_enum_t ap);

#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT

/**
 * Functions to be defined by OS host
 */

/**
 * @brief disable the MPU
 *
 * @return 0 on success
 * @return <0 on failure or no MPU present
 */
int mpu_disable(void);

/**
 * @brief enable the MPU
 *
 * @return 0 on success
 * @return <0 on failure or no MPU present
 */
extern int mpu_enable(void);

/**
 * @brief test if the MPU is enabled
 *
 * @return true if enabled
 * @return false if disabled
 */
extern bool mpu_enabled(void);

/**
 * @brief configure the base address and attributes for an MPU region
 *
 * @param[in]   region  MPU region to configure (0 <= @p region < MPU_NUM_REGIONS)
 * @param[in]   base    base address in RAM (aligned to the size specified within @p attr)
 * @param[in]   attr    attribute word generated by MPU_ATTR()
 *
 * @return 0 on success
 * @return <0 on failure or no MPU present
 */
extern int mpu_configure(uint_fast8_t region, uintptr_t base, uint_fast32_t attr);

#endif /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_MPU_DRIVER_H */
