#include <uk/futex.h>
#include <uk/syscall.h>
#include <uk/arch/atomic.h>
#include <uk/thread.h>
#include <uk/list.h>
#include <stdio.h>
#include <uk/assert.h>
#include <uk/print.h>
#include <errno.h>

static UK_LIST_HEAD(futex_list);

static int futex_wait(uint32_t *uaddr, uint32_t val, const struct timespec *tm)
{
	struct uk_thread *current = uk_thread_current();
	struct uk_futex f = {.uaddr = uaddr, .thread = current};
	int ret;

	if (ukarch_load_n(uaddr) == val) {

		/* Enqueue thread to wait list */
		uk_list_add_tail(&f.list_node, &futex_list);

		if (tm)
			/* Block for at most timeout nanosecs */
			uk_thread_block_timeout(current, tm->tv_nsec);
		else
			/* Block indefinitely */
			uk_thread_block(current);

		uk_sched_yield();

		/* Thread is no longer blocked. Remove it from the futex list */
		uk_list_del(&f.list_node);

		/* TODO: Should return -ETIMEDOUT if appropriate */
		return 0;
	}

	/* Futex word does not contain expected val */
	return -EAGAIN;
}

static int futex_wake(uint32_t *uaddr, uint32_t val)
{
	struct uk_list_head *itr, *tmp;
	struct uk_futex *f;
	int count = 0;

	if (val == 0)
		return 0;

	uk_list_for_each_safe(itr, tmp, &futex_list) {
		f = uk_list_entry(itr, struct uk_futex, list_node);

		if (f->uaddr == uaddr) {
			uk_thread_wakeup(f->thread);

			/* Wake at most val threads */
			if (++count >= val)
				break;
		}
	}

	return count;
}

static int futex_cmp_requeue(uint32_t *uaddr, uint32_t val,
			     const struct timespec *timeout, uint32_t *uaddr2,
			     uint32_t val3)
{
	/* For several blocking operations, the timeout argument is a
	   pointer to a timespec structure that specifies a timeout for the
	   operation.  However,  notwithstanding the prototype shown above,
	   for some operations, the least significant four bytes of this
	   argument are instead used as an integer whose meaning is
	   determined by the operation.  For these operations, the kernel
	   casts the timeout value first to unsigned long, then to uint32_t,
	   and in the remainder of this page, this argument is referred to
	   as val2 when interpreted in this fashion.*/
	// TODO: probably better to use val2 directly from call to futex
	unsigned long tmp = (unsigned long)(timeout->tv_nsec);
	uint32_t val2 = tmp & 0xFFFFFFFF;

	if (!((uint32_t)val3 == ukarch_load_n(uaddr))) {
		return -EAGAIN;
	}

	int ret = futex_wake(uaddr, val);
	// TODO:
	/*uint32_t waiters_uaddr2 = 0;
	if (val > ret) {

	}*/
	/*Otherwise, the operation
	          wakes up a maximum of val waiters that are waiting on the
	          futex at uaddr.  If there are more than val waiters, then
	          the remaining waiters are removed from the wait queue of
	          the source futex at uaddr and added to the wait queue of
	          the target futex at uaddr2.  The val2 argument specifies
	          an upper limit on the number of waiters that are requeued
	          to the futex at uaddr2.*/
	return 0;
}

int do_futex(uint32_t *uaddr, int futex_op, uint32_t val,
	     const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3)
{
	switch (futex_op) {
	case FUTEX_WAIT:
	case FUTEX_WAIT_PRIVATE:
		return futex_wait(uaddr, val, timeout);

	case FUTEX_WAKE:
	case FUTEX_WAKE_PRIVATE:
		return futex_wake(uaddr, val);

	case FUTEX_FD:
	case FUTEX_REQUEUE:
		return -ENOSYS;

	case FUTEX_CMP_REQUEUE:
		return futex_cmp_requeue(uaddr, val, timeout, uaddr2, val3);

	default:
		return -ENOSYS;
		// TODO: other operations?
	}
}

UK_SYSCALL_DEFINE(int, futex, uint32_t *, uaddr, int, futex_op, uint32_t, val,
		  const struct timespec *, timeout, uint32_t *, uaddr2,
		  uint32_t, val3)
{
	int error;

	error = do_futex(uaddr, futex_op, val, timeout, uaddr2, val3);
	if (error < 0)
		goto out_errno;

	return error;

out_errno:
	errno = -error;
	return -1;
}
