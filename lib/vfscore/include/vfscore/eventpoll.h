/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Marc Rittinghaus <marc.rittinghaus@kit.edu>
 *
 * Copyright (c) 2021, Karlsruhe Institute of Technology (KIT)
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

#ifndef _VFSCORE_EVENTPOLL_H_
#define _VFSCORE_EVENTPOLL_H_

#include <uk/essentials.h>
#include <uk/arch/time.h>
#include <uk/arch/spinlock.h>
#include <uk/list.h>
#include <uk/mutex.h>
#include <uk/alloc.h>
#include <uk/assert.h>
#include <sys/epoll.h>
#include <stdint.h>

struct vfscore_file;

#ifndef EPOLLNVAL
#define EPOLLNVAL	0x00000020	/* Invalid request: fd not open */
#endif

/* Eventpoll context block */
struct eventpoll_cb {
	/* If the driver registers this eventpoll to signal events, it must
	 * supply an unregister callback that safely removes the eventpoll.
	 * The driver should register this eventpoll only on the first call
	 * to VOP_POLL(). The driver may check the callback for NULL or use
	 * the additional data field for this purpose. After a call to the
	 * unregister callback the driver must not access this data structure
	 * anymore!
	 *
	 * N.B. The driver must serialize calls to eventpoll_signal() with the
	 * unregister operation to avoid races where an eventpoll is destroyed
	 * while the driver is signaling an event.
	 */
	void (*unregister)(struct eventpoll_cb *ecb);

	/* Optional context for driver */
	void *data;

	/* Optional link for the driver to add this cb into a list */
	struct uk_list_head cb_link;
};

/* Eventpoll file description */
struct eventpoll_fd {
	/* Context block for driver to implement asynchronous signaling */
	struct eventpoll_cb cb;

	/* Parent eventpoll */
	struct eventpoll *ep;

	/* Reference to the monitored file */
	struct vfscore_file *vfs_file;

	/* Due to dup() we can have multiple fds point to the same file. We
	 * thus have to store the fd that has been used to register the file
	 * for eventpoll. We use it in epoll_ctl() to match the requested fd.
	 */
	int fd;

	/* User-supplied requested events and data */
	struct epoll_event event;

	/* Used to link into monitored fd list */
	struct uk_list_head fd_link;

	/* Used to link into triggered list which is scanned by
	 * eventpoll_wait() to find pending events. Being in the list, does not
	 * guarantee that events are still pending.
	 */
	struct uk_list_head tr_link;

	/* Used to link into the file's epoll list so we are informed when
	 * the file is closed.
	 */
	struct uk_list_head f_link;
};

static inline void eventpoll_fd_init(struct eventpoll_fd *efd,
				     struct vfscore_file *fp, int fd,
				     const struct epoll_event *event)
{
	UK_ASSERT(efd);
	UK_ASSERT(fp);
	UK_ASSERT(event);

	efd->ep = NULL;
	efd->vfs_file = fp;
	efd->fd = fd;
	efd->event = *event;

	efd->cb.unregister = NULL;
	efd->cb.data = NULL;
	UK_INIT_LIST_HEAD(&efd->cb.cb_link);

	UK_INIT_LIST_HEAD(&efd->fd_link);
	UK_INIT_LIST_HEAD(&efd->tr_link);
	UK_INIT_LIST_HEAD(&efd->f_link);
}

struct eventpoll {
	/* Lock to serialize operations on the fd and tr lists.
	 * TODO: The single lock will be a bottleneck at some point if
	 * we have many fds watched, signaled, and waited for. Add a seperate
	 * lock for the triggered list. We might also get into trouble if
	 * a thread is blocked in eventpoll_signal() and eventpoll_signal()
	 * is called from an IRQ handler. So the tr list should be protected
	 * with a reader-writer spinlock.
	 */
	struct uk_mutex fd_lock;

	/* List of monitored fds */
	struct uk_list_head fd_list;

	/* List of triggered fds */
	struct uk_list_head tr_list;

	/* Queue of threads waiting on this eventpoll */
	struct uk_waitq wq;
};

static inline void eventpoll_init(struct eventpoll *ep)
{
	UK_ASSERT(ep);

	uk_mutex_init(&ep->fd_lock);
	UK_INIT_LIST_HEAD(&ep->fd_list);
	UK_INIT_LIST_HEAD(&ep->tr_list);
	uk_waitq_init(&ep->wq);
}

void eventpoll_fini(struct eventpoll *ep, struct uk_alloc *a);

int eventpoll_add(struct eventpoll *ep, struct uk_alloc *a, int fd,
		  struct vfscore_file *fp, const struct epoll_event *event);
int eventpoll_mod(struct eventpoll *ep, int fd,
		  const struct epoll_event *event);
int eventpoll_del(struct eventpoll *ep, struct uk_alloc *a, int fd);

void eventpoll_notify_close(struct vfscore_file *fp);

void eventpoll_add_unsafe(struct eventpoll *ep, struct eventpoll_fd *efd);
void eventpoll_del_unsafe(struct eventpoll_fd *efd);

int eventpoll_wait(struct eventpoll *ep, struct epoll_event *events,
		   int maxevents, const __nsec *timeout);
/* eventpoll_signal() must not be called from withing an IRQ context */
void eventpoll_signal(struct eventpoll_cb *ecb, unsigned int revents);

#endif /* _VFSCORE_EVENTPOLL_H_ */
