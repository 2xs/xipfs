#include "include/shared_api.h"

#ifdef XIPFS_ENABLE_SAFE_EXEC_SUPPORT

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

/**
 * @internal
 *
 * We use this function and end_xipfs_shared_api_code_in_function
 * to create an MPU-compatible memory block, named xipfs_shared_api_code_in :
 * - start_xipfs_shared_api_code_in_function gives us the required alignment
 * - end_xipfs_shared_api_code_in_function gives us the required size
 *
 * This section, and its properties, are kept in the final linkscript through
 * xipfs_shared_api_code_out section.
 *
 * We don't rely onto full linkscripts implementations because the latter can be
 * shared with other applications and boards, leading to building failures
 * such as rom overflows in RIOT OS.
 *
 * With the current approach, xipfs_shared_api_code_out section in linkscript
 * is only filled when compiling this code, and is left empty when not.
 *
 * @see end_xipfs_shared_api_code_in_function
 */
__attribute__((section(".xipfs_shared_api_code_in"), aligned(XIPFS_SHARED_API_CODE_ALIGNMENT), used, naked))
static void start_xipfs_shared_api_code_in_function(void){}

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in")))
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

__attribute__((section(".xipfs_shared_api_code_in"), aligned(XIPFS_SHARED_API_CODE_SIZE), used, naked))
static void end_xipfs_shared_api_code_in_function(void){}

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

#endif /* XIPFS_ENABLE_SAFE_EXEC_SUPPORT */
