/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 * Copyright (c) 2022, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __UK_THREAD_TCB_IMPL_H__
#define __UK_THREAD_TCB_IMPL_H__

#include <uk/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This header provides prototypes and helpers for integrating a custom
 * thread control block to `libuksched` threads.
 * A library (e.g., libc, pthread) that enables the integration specifies
 * the required bytes as TCB via its `Makefile.uk` by calling the
 * `uksched_tcb_reserve` function:
 *
 *   $(eval $(call uksched_tcb_reserve,<tcb_size_in_bytes>))
 *
 * `libuksched` will then link to the two functions `uk_thread_uktcb_init()`,
 * and `uk_thread_uktcb_fini()` that must be provided by the TCB library.
 *
 * NOTE: The following definitions are only available if there is
 *       one library available in the build that requires thread
 *       control blocks (TCBs). Only a single library of a build is
 *       able to reserve space for the TCB.
 * HINT: A library requiring a TCB for each thread should use
 *       `uksched_tcb_reserve` in a its `Makefile.uk` which will make
 *       the following symbols becoming used.
 */

#if LIBUKSCHED_HAVE_TCB
/**
 * Called by `libuksched` as soon as a thread is created or initialized
 * that has a Unikraft TLS allocated.
 * It is intended that a libc or a thread abstraction library that require
 * a TCB will provide this function symbol.
 *
 * @param uktcb Reference to reserved space to custom thread control block
 *              that should be initialized. Please note that only the TCB
 *              self-pointer will be already set. The rest of bytes
 *              in this area is not zero'd out.
 * @return
 *   - (>=0): Success, uktcb is initialized.
 *   - (<0): Negative error code, thread creation is canceled.
 */
int uk_thread_uktcb_init(void *uktcb);

/**
 * Called by `libuksched` as soon as a thread is released or uninitialized
 * that has a Unikraft TLS allocated.
 * It is intended that a libc or a thread abstraction library that require
 * a TCB will provide this function symbol.
 *
 * @param uktcb Reference to reserved space to custom thread control block
 *              that should be uninitialized.
 */
void uk_thread_uktcb_fini(void *uktcb);
#endif /* LIBUKSCHED_HAVE_TCB */

/**
 * Macro that returns the TCB of a (foreign) thread with a Unikraft TLS (UKTLS).
 *
 * NOTE: As defined by the TLS-ABI, the first field in a TCB is a self pointer.
 *       If no custom TCB is configured, only the self-pointer can be found
 *       at the returned address.
 */
#define uk_thread_uktcb(thread)						\
	({								\
		UK_ASSERT((thread)->flags & UK_THREADF_UKTLS);		\
									\
		((void *) (thread)->uktlsp);				\
	})

/**
 * Returns the configured space reserved for TCB on Unikraft TLS
 * It basically returns the given size in bytes with the Makefile
 * function `uksched_tcb_reserve` (see `Makefile.rules`).
 *
 * NOTE: A minimal TCB contains only a self-pointer. If no custom TCB
 *       is configured, `uk_thread_uktcb_size()` returns the size of
 *       a pointer.
 */
size_t uk_thread_uktcb_size(void);

#ifdef __cplusplus
}
#endif

#endif /* __UK_THREAD_TCB_IMPL_H__ */
