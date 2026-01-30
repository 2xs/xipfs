#include "include/shared_api.h"

/**
 * @internal
 *
 * @def STR_HELPER
 *
 * @brief Used for preprocessing in asm statements
 */
#define STR_HELPER(x) #x

/**
 * @internal
 *
 * @def STR
 *
 * @brief Used for preprocessing in asm statements
 */
#define STR(x)        STR_HELPER(x)

__attribute__((section(".shared_api_code")))
static void exit_wrapper(int status) {
    __asm__ volatile(
        "mov r0, %0                            \n"
        "mov r1, %1                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        :
        :"r"(XIPFS_SYSCALL_EXIT), "r"(status)
        : "r0", "r1"
    );
}

__attribute__((section(".shared_api_code")))
static int vprintf_wrapper(const char * format, va_list va) {
    int res;

    __asm__ volatile(
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "mov r2, %3                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_VPRINTF), "r"(format), "r"(va)
        : "r0", "r1", "r2"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static int get_temp_wrapper(void) {
    int res;

    __asm__ volatile (
        "mov r0, %1                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_GET_TEMP)
        : "r0"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static int isprint_wrapper(int character) {
    int res;

    __asm__ volatile(
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_ISPRINT), "r"(character)
        : "r0", "r1"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static long strtol_wrapper(const char *str, char **endptr, int base) {
    long res;

    __asm__ volatile (
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "mov r2, %3                            \n"
        "mov r3, %4                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_STRTOL), "r"(str), "r"(endptr), "r"(base)
        : "r0", "r1", "r2", "r3"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static int get_led_wrapper(int pos) {
    int res;

    __asm__ volatile(
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_GET_LED), "r"(pos)
        : "r0", "r1"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static int set_led_wrapper(int pos, int val) {
    int res;

    __asm__ volatile (
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "mov r2, %3                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_SET_LED), "r"(pos), "r"(val)
        : "r0", "r1", "r2"
    );


    return res;
}

__attribute__((section(".shared_api_code")))
static ssize_t copy_file_wrapper(const char *name, void *buf, size_t nbyte) {
    ssize_t res;

    __asm__ volatile(
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "mov r2, %3                            \n"
        "mov r3, %4                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_COPY_FILE), "r"(name), "r"(buf), "r"(nbyte)
        : "r0", "r1", "r2", "r3"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static int get_file_size_wrapper(const char *name, size_t *size) {
    int res;

    __asm__ volatile(
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "mov r2, %3                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_GET_FILE_SIZE), "r"(name), "r"(size)
        : "r0", "r1", "r2"
    );

    return res;
}

__attribute__((section(".shared_api_code")))
static void *memset_wrapper(void *m, int c, size_t n) {
    void *res;

    __asm__ volatile(
        "mov r0, %1                            \n"
        "mov r1, %2                            \n"
        "mov r2, %3                            \n"
        "mov r3, %4                            \n"
        "svc #" STR(XIPFS_SYSCALL_SVC_NUMBER) "\n"
        "mov %0, r0                            \n"
        : "=r"(res)
        : "r"(XIPFS_SYSCALL_MEMSET), "r"(m), "r"(c), "r"(n)
        : "r0", "r1", "r2", "r3"
    );

    return res;
}

__attribute__((section(".shared_api_data")))
const void *xipfs_safe_exec_syscalls_wrappers[XIPFS_SYSCALL_MAX] = {
    [         XIPFS_SYSCALL_EXIT] = exit_wrapper,
    [      XIPFS_SYSCALL_VPRINTF] = vprintf_wrapper,
    [     XIPFS_SYSCALL_GET_TEMP] = get_temp_wrapper,
    [      XIPFS_SYSCALL_ISPRINT] = isprint_wrapper,
    [       XIPFS_SYSCALL_STRTOL] = strtol_wrapper,
    [      XIPFS_SYSCALL_GET_LED] = get_led_wrapper,
    [      XIPFS_SYSCALL_SET_LED] = set_led_wrapper,
    [    XIPFS_SYSCALL_COPY_FILE] = copy_file_wrapper,
    [XIPFS_SYSCALL_GET_FILE_SIZE] = get_file_size_wrapper,
    [       XIPFS_SYSCALL_MEMSET] = memset_wrapper
};
