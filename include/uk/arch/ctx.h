/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 * Copyright (c) 2021, NEC Laboratories Europe GmbH, NEC Corporation.
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

#ifndef __UKARCH_CTX_H__
#define __UKARCH_CTX_H__

#include <uk/arch/types.h>
#ifndef __ASSEMBLY__
#include <uk/config.h>
#if CONFIG_LIBUKDEBUG
#include <uk/assert.h>
#else /* !CONFIG_LIBUKDEBUG */
#define UK_ASSERT(...) do {} while (0)
#endif /* !CONFIG_LIBUKDEBUG */
#include <uk/essentials.h>
#include <uk/asm/ctx.h>
#endif /*!__ASSEMBLY__*/

#define UKARCH_CTX_OFFSETOF_IP 0
#if (defined __PTR_IS_16)
#define UKARCH_CTX_OFFSETOF_SP 2
#elif (defined __PTR_IS_32)
#define UKARCH_CTX_OFFSETOF_SP 4
#elif (defined __PTR_IS_64)
#define UKARCH_CTX_OFFSETOF_SP 8
#endif

#ifndef __ASSEMBLY__
struct ukarch_ctx {
	__uptr ip;	/**< instruction pointer */
	__uptr sp;	/**< stack pointer */
} __packed;

/*
 * Context function are not allowed to return
 */
typedef void (*ukarch_ctx_entry0)(void) __noreturn;
typedef void (*ukarch_ctx_entry1)(long) __noreturn;
typedef void (*ukarch_ctx_entry2)(long, long) __noreturn;

void ukarch_ctx_init_entry0(struct ukarch_ctx *ctx,
			    __uptr sp, int keep_regs,
			    ukarch_ctx_entry0 entry);
void ukarch_ctx_init_entry1(struct ukarch_ctx *ctx,
			    __uptr sp, int keep_regs,
			    ukarch_ctx_entry1 entry, long arg);
void ukarch_ctx_init_entry2(struct ukarch_ctx *ctx,
			    __uptr sp, int keep_regs,
			    ukarch_ctx_entry2 entry, long arg0, long arg1);

/* Just sets ip and sp */
static inline void ukarch_ctx_init_bare(struct ukarch_ctx *ctx,
					__uptr sp, __uptr ip)
{
	UK_ASSERT(ctx);

	(*ctx) = (struct ukarch_ctx){ .sp = sp, .ip = ip };
}

/* Resets registers before executing ip */
static inline void ukarch_ctx_init(struct ukarch_ctx *ctx,
				   __uptr sp, __uptr ip)
{
	ukarch_ctx_init_entry0(ctx, sp, 0, (ukarch_ctx_entry0) ip);
}

#define ukarch_rctx_stackpush(ctx, value)				\
	do {								\
		(ctx)->sp = ukarch_rstack_push((ctx)->sp, (value));	\
	} while(0)

/* non-inline version of context switch that online load and restores callee registers */
extern void ukarch_ctx_switch(struct ukarch_ctx *store, struct ukarch_ctx *load);

/**
 * State of extended context, like additional CPU registers and units
 * (e.g., floating point, vector registers)
 */
struct ukarch_ectx;

__sz ukarch_ectx_size(void);
__sz ukarch_ectx_align(void);
void ukarch_ectx_init(struct ukarch_ectx *state);
void ukarch_ectx_store(struct ukarch_ectx *state);
void ukarch_ectx_load(struct ukarch_ectx *state);

#endif /* !__ASSEMBLY__ */
#endif /* __UKARCH_CTX_H__ */
