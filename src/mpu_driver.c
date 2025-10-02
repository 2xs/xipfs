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

#include "include/mpu_driver.h"

#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT

/* https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2 */
static bool is_power_of_two(uint32_t v) {
    return v && !(v & (v - 1));
}

/* https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog */
static uint32_t log2Integer(uint32_t v) {
    uint32_t result = (v & 0xAAAAAAAA) != 0;
    result |= ((v & 0xFFFF0000) != 0) << 4;
    result |= ((v & 0xFF00FF00) != 0) << 3;
    result |= ((v & 0xF0F0F0F0) != 0) << 2;
    result |= ((v & 0xCCCCCCCC) != 0) << 1;

    return result;
}

#endif /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */

int xipfs_mpu_configure_region(
    xipfs_mpu_region_enum_t mpu_region, void *address, uint32_t size,
    xipfs_mpu_region_xn_enum_t xn, xipfs_mpu_region_ap_enum_t ap) {

#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT

    if (   (mpu_region < XIPFS_MPU_REGION_ENUM_FIRST)
        || (mpu_region > XIPFS_MPU_REGION_ENUM_LAST)) {
        return -1;
    }

    if (xn > XIPFS_MPU_REGION_EXC_LAST) {
        return -1;
    }

    if (   (ap > XIPFS_MPU_REGION_AP_LAST)
        || (ap == XIPFS_MPU_REGION_AP_RESERVED)) {
        return -1;
    }

    if ((size < 32) || (is_power_of_two(size) == false)) {
        return -1;
    }

    if (((uintptr_t)address) % size != 0) {
        return -1;
    }

    uint32_t log2_size = log2Integer(size) - 1;

    /*
     * @see ARMv7-M Architecture Reference Manual,
     * B3.5.9 MPU Region Attribute and Size Register, MPU_RASR
     */
    uint32_t attributes =
        (((uint32_t)xn) << 28)  |
        (((uint32_t)ap) << 24)  |
        (0  << 19)  |
        (1  << 18)  |
        (0  << 17)  |
        (1  << 16)  |
        (log2_size << 1);

    return mpu_configure(mpu_region,(uintptr_t)address, attributes);

#else /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */
    (void)mpu_region;
    (void)address;
    (void)size;
    (void)xn;
    (void)ap;

    return -1;

#endif /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */
}
