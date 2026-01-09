/*
 * sys_arch.c - lwIP system abstraction layer for bare-metal
 *
 * With NO_SYS=1, most functions are not needed.
 * Only sys_now() is required for timeouts.
 */

#include "lwip/sys.h"
#include "timer.h"

/* sys_now() is implemented in timer.c */

/* Critical section protection - single-threaded, so no-op */
#if SYS_LIGHTWEIGHT_PROT
sys_prot_t sys_arch_protect(void) {
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
}
#endif
