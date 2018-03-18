/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2018 Joyent, Inc.
 */

#ifndef _SYS_PC_MMU_H
#define	_SYS_PC_MMU_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-dependent MMU routines and types for real x86 hardware.
 *
 * WARNING: this header file is used by both dboot and i86pc, so don't go using
 * normal kernel headers.
 */

#define	IN_HYPERVISOR_VA(va) (__lintzero)

void reload_cr3(void);

#define	pa_to_ma(pa) (pa)
#define	ma_to_pa(ma) (ma)
#define	pfn_to_mfn(pfn) (pfn)
#define	mfn_to_pfn(mfn)	(mfn)

#ifndef _BOOT

extern uint64_t kpti_safe_cr3;

#define	INVPCID_ADDR		(0)
#define	INVPCID_ID		(1)
#define	INVPCID_ALL_GLOBAL	(2)
#define	INVPCID_ALL_NONGLOBAL	(3)

extern void invpcid_insn(uint64_t, uint64_t, uintptr_t);
extern void tr_mmu_flush_user_range(uint64_t, size_t, size_t, uint64_t);

#if defined(__GNUC__)
#include <asm/mmu.h>
#endif

#endif /* !_BOOT */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_PC_MMU_H */
