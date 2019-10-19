/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2019 Joyent, Inc.
 */

/*
 * Linux aio syscall support.
 *
 * The Linux story around the io_* syscalls is very confusing. The io_* syscalls
 * are not exposed via glibc and in fact, glibc seems to implement its own aio
 * without using the io_* syscalls at all. However, there is the libaio library
 * which uses the io_* syscalls, although its implementation of the io_*
 * functions (with the same names!) is different from the syscalls themselves,
 * and it uses different definitions for some of the structures involved.
 *
 * These syscalls are documented to use an aio_context_t for the context
 * parameter. On Linux this is a ulong_t. The contexts live in the kernel
 * address space and are looked up using the aio_context_t parameter. However,
 * the Linux libaio library, which is a consumer of the io_* syscalls, abuses
 * the context by assuming it can be used as a pointer into memory that is
 * mapped into the process. To accomodate this abomination we map a page of
 * anonymous memory and expose the context to user-land as a pointer offset
 * into that page. The page itself is never used by our code and our internal
 * context ID is simply an integer we calculate based on the page pointer
 * offset.
 *
 * Most applications never use aio, so we don't want an implementation that
 * adds overhead to every process, but on the other hand, when an application is
 * using aio, it is for performance reasons and we want to be as efficient as
 * possible. In particular, we don't want to dynamically allocate resources
 * in the paths that enqueue I/O. Instead, we pre-allocate the resources
 * we may need when the application performs the io_setup call and keep the
 * io_submit and io_getevents calls streamlined.
 *
 * The general approach here is inspired by the native aio support provided by
 * libc in user-land. We have worker threads that pick up pending work from
 * the context "lxioctx_pending" list and synchronously issue the operation in
 * the control block. When the operation completes, the thread places the
 * control block into the context "lxioctx_done" list for later consumption by
 * io_getevents. The thread will then attempt to service another pending
 * operation or wait for more work to arrive.
 *
 * The control blocks on the pending or done lists are referenced by an
 * lx_io_elem_t struct. This simply holds a pointer to the user-land control
 * block and the result of the operation. These elements are pre-allocated at
 * io_setup time and stored on the context "lxioctx_free" list.
 *
 * io_submit pulls elements off of the free list, places them on the pending
 * list and kicks a worker thread to run. io_getevents pulls elements off of
 * the done list, sets up an event to return, and places the elements back
 * onto the free list.
 *
 * The worker threads are pre-allocated at io_setup time. These are LWP's
 * that are part of the process, but never leave the kernel. The number of
 * LWP's is allocated based on the nr_events argument to io_setup. Because
 * this argument can theoretically be large (up to LX_AIO_MAX_NR), we want to
 * pre-allocate enough threads to get good I/O concurrency, but not overdo it.
 * For a small nr_events (<= lx_aio_base_workers) we pre-allocate as many
 * threads as nr_events so that all of the the I/O can run in parallel. Once
 * we exceed lx_aio_base_workers, we scale up the number of threads by 2, until
 * we hit the maximum at lx_aio_max_workers. See the code in io_setup for more
 * information.
 *
 * Because the worker threads never leave the kernel, they are marked with the
 * TP_KTHREAD bit so that /proc operations essentially ignore them. We also tag
 * the brand lwp flags with the BR_AIO_LWP bit so that these threads never
 * appear in the lx /proc. Aside from servicing aio submissions, the worker
 * threads don't participate in most application-initiated operations. Forking
 * is a special case for the workers. The Linux fork(2) and vfork(2) behavior
 * always forks only a single thread; the caller. However, during cfork() the
 * system attempts to quiesce all threads by calling holdlwps(). The workers
 * check for SHOLDFORK and SHOLDFORK1 in their loops and suspend themselves ala
 * holdlwp() if the process forks.
 *
 * It is hard to make any generalized statements about how the aio syscalls
 * are used in production. MySQL is one of the more popular consumers of aio
 * and in the default configuration it will create 10 contexts with a capacity
 * of 256 I/Os (io_setup nr_events) and 1 context with a capacity of 100 I/Os.
 * Another application we've seen will create 8 contexts, each with a capacity
 * of 128 I/Os. In practice 1-7 was the typical number of in-flight I/Os.
 *
 * The default configuration for MySQL uses 4 read and 4 write threads. Each
 * thread has an associated context. MySQL also allocates 3 additional contexts,
 * so in the default configuration it will only use 11, but the number of
 * read and write threads can be tuned up to a maximum of 64. We can expand
 * a process's number of contexts up to a maximum of LX_IOCTX_CNT_MAX, which
 * is significantly more than we've ever seen in use.
 *
 * According to www.kernel.org/doc/Documentation/sysctl/fs.txt, the
 * /proc/sys/fs entries for aio are:
 * - aio-nr: The total of all nr_events values specified on the io_setup
 *           call for every active context.
 * - aio-max-nr: The upper limit for aio-nr
 * aio-nr is tracked as a zone-wide value. We keep aio-max-nr limited to
 * LX_AIO_MAX_NR, which matches Linux and provides plenty of headroom for the
 * zone.
 */

#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/brand.h>
#include <sys/sysmacros.h>
#include <sys/sdt.h>
#include <sys/procfs.h>
#include <sys/eventfd.h>

#include <sys/lx_brand.h>
#include <sys/lx_syscalls.h>
#include <sys/lx_misc.h>
#include <lx_errno.h>

/* These constants match Linux */
#define	LX_IOCB_FLAG_RESFD		0x0001
#define	LX_IOCB_CMD_PREAD		0
#define	LX_IOCB_CMD_PWRITE		1
#define	LX_IOCB_CMD_FSYNC		2
#define	LX_IOCB_CMD_FDSYNC		3
#define	LX_IOCB_CMD_PREADX		4
#define	LX_IOCB_CMD_POLL		5
#define	LX_IOCB_CMD_NOOP		6
#define	LX_IOCB_CMD_PREADV		7
#define	LX_IOCB_CMD_PWRITEV		8

#define	LX_KIOCB_KEY			0

/*
 * Base and max. number of contexts/process. Note that we currently map one
 * page to manage the user-level context ID, so that code must be adjusted if
 * LX_IOCTX_CNT_MAX is ever enlarged. Currently, this is the limit for the
 * number of 64-bit pointers in one 4k page.
 */
#define	LX_IOCTX_CNT_BASE	16
#define	LX_IOCTX_CNT_MAX	512

/*
 * Max number of control block pointers, or lx_io_event_t's, to allocate on the
 * stack in io_submit or io_getevents.
 */
#define	MAX_ALLOC_ON_STACK	128
#define	alloca(x)		__builtin_alloca(x)
extern void *__builtin_alloca(size_t);

/* The context is an offset within the ctxpage we mapped */
#define	CTXID_TO_PTR(L, I)	((L)->l_io_ctxpage + ((I) * sizeof (uintptr_t)))
#define	PTR_TO_CTXID(L, P)	((int)((uintptr_t)(P) - (L)->l_io_ctxpage) / \
				sizeof (uintptr_t))

typedef ulong_t lx_aio_context_t;

uint_t	lx_aio_base_workers = 16;	/* num threads/context before scaling */
uint_t	lx_aio_max_workers = 32;	/* upper limit on threads/context */

/*
 * Internal representation of an aio context.
 */
typedef struct lx_io_ctx {
	boolean_t	lxioctx_shutdown;	/* context is being destroyed */
	uint_t		lxioctx_maxn;		/* nr_events from io_setup */
	uint_t		lxioctx_in_use;		/* reference counter */
	kmutex_t	lxioctx_f_lock;		/* free list lock */
	uint_t		lxioctx_free_cnt;	/* num. elements in free list */
	list_t		lxioctx_free;		/* free list */
	kmutex_t	lxioctx_p_lock;		/* pending list lock */
	kcondvar_t	lxioctx_pending_cv;	/* pending list cv */
	list_t		lxioctx_pending;	/* pending list */
	kmutex_t	lxioctx_d_lock;		/* done list lock */
	kcondvar_t	lxioctx_done_cv;	/* done list cv */
	uint_t		lxioctx_done_cnt;	/* num. elements in done list */
	list_t		lxioctx_done;		/* done list */
} lx_io_ctx_t;

/*
 * Linux binary definition of an I/O event.
 */
typedef struct lx_io_event {
	uint64_t	lxioe_data;	/* data payload */
	uint64_t	lxioe_object;	/* object of origin */
	int64_t		lxioe_res;	/* result code */
	int64_t		lxioe_res2;	/* "secondary" result (WTF?) */
} lx_io_event_t;

/*
 * Linux binary definition of an I/O control block.
 */
typedef struct lx_iocb {
	uint64_t	lxiocb_data;		/* data payload */
	uint32_t	lxiocb_key;		/* must be LX_KIOCB_KEY (!) */
	uint32_t	lxiocb_reserved1;
	uint16_t	lxiocb_op;		/* operation */
	int16_t		lxiocb_reqprio;		/* request priority */
	uint32_t	lxiocb_fd;		/* file descriptor */
	uint64_t	lxiocb_buf;		/* data buffer */
	uint64_t	lxiocb_nbytes;		/* number of bytes */
	int64_t		lxiocb_offset;		/* offset in file */
	uint64_t	lxiocb_reserved2;
	uint32_t	lxiocb_flags;		/* LX_IOCB_FLAG_* flags */
	uint32_t	lxiocb_resfd;		/* eventfd fd, if any */
} lx_iocb_t;

typedef struct lx_io_elem {
	list_node_t	lxioelem_link;
	uint16_t	lxioelem_op;		/* operation */
	uint16_t	lxioelem_flags;		/* bits from lxiocb_flags */
	int		lxioelem_fd;		/* file descriptor */
	file_t		*lxioelem_fp;		/* getf() file pointer */
	int		lxioelem_resfd;		/* RESFD file descriptor */
	file_t		*lxioelem_resfp;	/* RESFD getf() file pointer */
	void		*lxioelem_buf;		/* data buffer */
	uint64_t	lxioelem_nbytes;	/* number of bytes */
	int64_t		lxioelem_offset;	/* offset in file */
	uint64_t	lxioelem_data;
	ssize_t		lxioelem_res;
	void		*lxioelem_cbp;		/* ptr to iocb in userspace */
} lx_io_elem_t;

/* From lx_rw.c */
extern ssize_t lx_pread_fp(file_t *, void *, size_t, off64_t);
extern ssize_t lx_pwrite_fp(file_t *, void *, size_t, off64_t);

/* From common/syscall/rw.c */
extern int fdsync(int, int);
/* From common/os/grow.c */
extern caddr_t smmap64(caddr_t, size_t, int, int, int, off_t);

/*
 * Given an aio_context ID, return our internal context pointer with an
 * additional ref. count, or NULL if cp not found.
 */
static lx_io_ctx_t *
lx_io_cp_hold(lx_aio_context_t cid)
{
	int id;
	lx_proc_data_t *lxpd = ptolxproc(curproc);
	lx_io_ctx_t *cp;

	mutex_enter(&lxpd->l_io_ctx_lock);

	if (lxpd->l_io_ctxs == NULL) {
		ASSERT(lxpd->l_io_ctx_cnt == 0);
		ASSERT(lxpd->l_io_ctxpage == (uintptr_t)NULL);
		goto bad;
	}

	id = PTR_TO_CTXID(lxpd, cid);
	if (id < 0 || id >= lxpd->l_io_ctx_cnt)
		goto bad;

	if ((cp = lxpd->l_io_ctxs[id]) == NULL)
		goto bad;

	if (cp->lxioctx_shutdown)
		goto bad;

	atomic_inc_32(&cp->lxioctx_in_use);
	mutex_exit(&lxpd->l_io_ctx_lock);
	return (cp);

bad:
	mutex_exit(&lxpd->l_io_ctx_lock);
	return (NULL);
}

/*
 * Release a hold on the context and clean up the context if it was the last
 * hold.
 */
static void
lx_io_cp_rele(lx_io_ctx_t *cp)
{
	lx_proc_data_t *lxpd = ptolxproc(curproc);
	lx_zone_data_t *lxzd;
	int i;
	lx_io_elem_t *ep;

	mutex_enter(&lxpd->l_io_ctx_lock);
	ASSERT(cp->lxioctx_in_use >= 1);
	if (cp->lxioctx_in_use > 1) {
		atomic_dec_32(&cp->lxioctx_in_use);
		/* wake all threads waiting on context rele */
		cv_broadcast(&lxpd->l_io_destroy_cv);
		mutex_exit(&lxpd->l_io_ctx_lock);
		return;
	}

	/*
	 * We hold the last ref.
	 */
	for (i = 0; i < lxpd->l_io_ctx_cnt; i++) {
		if (lxpd->l_io_ctxs[i] == cp) {
			lxpd->l_io_ctxs[i] = NULL;
			break;
		}
	}
	ASSERT(i < lxpd->l_io_ctx_cnt);
	/* wake all threads waiting on context destruction */
	cv_broadcast(&lxpd->l_io_destroy_cv);
	ASSERT(cp->lxioctx_shutdown == B_TRUE);

	mutex_exit(&lxpd->l_io_ctx_lock);

	/* can now decrement the zone's overall aio counter */
	lxzd = ztolxzd(curproc->p_zone);
	mutex_enter(&lxzd->lxzd_lock);
	VERIFY(cp->lxioctx_maxn <= lxzd->lxzd_aio_nr);
	lxzd->lxzd_aio_nr -= cp->lxioctx_maxn;
	mutex_exit(&lxzd->lxzd_lock);

	/*
	 * We have the only pointer to the context now. Free all
	 * elements from all three queues and the context itself.
	 */
	while ((ep = list_remove_head(&cp->lxioctx_free)) != NULL) {
		kmem_free(ep, sizeof (lx_io_elem_t));
	}

	/*
	 * During io_submit() we use getf() to get/validate the file pointer
	 * for the file descriptor in each control block. We do not releasef()
	 * the fd, but instead pass along the fd and file pointer to the worker
	 * threads. In order to manage this hand-off we use clear_active_fd()
	 * in the syscall path and then in our thread which takes over the file
	 * descriptor, we use a combination of set_active_fd() and releasef().
	 * Because our thread that is taking ownership of the fd has not called
	 * getf(), we first call set_active_fd(-1) to reserve a slot in the
	 * active fd array for ourselves.
	 */
	set_active_fd(-1);
	while ((ep = list_remove_head(&cp->lxioctx_pending)) != NULL) {
		set_active_fd(ep->lxioelem_fd);
		releasef(ep->lxioelem_fd);

		if (ep->lxioelem_flags & LX_IOCB_FLAG_RESFD) {
			set_active_fd(ep->lxioelem_resfd);
			releasef(ep->lxioelem_resfd);
		}

		kmem_free(ep, sizeof (lx_io_elem_t));
	}

	while ((ep = list_remove_head(&cp->lxioctx_done)) != NULL) {
		kmem_free(ep, sizeof (lx_io_elem_t));
	}

	ASSERT(list_is_empty(&cp->lxioctx_free));
	list_destroy(&cp->lxioctx_free);
	ASSERT(list_is_empty(&cp->lxioctx_pending));
	list_destroy(&cp->lxioctx_pending);
	ASSERT(list_is_empty(&cp->lxioctx_done));
	list_destroy(&cp->lxioctx_done);

	kmem_free(cp, sizeof (lx_io_ctx_t));
}

/*
 * Called by a worker thread to perform the operation specified in the control
 * block.
 *
 * Linux returns a negative errno in the event "lxioelem_res" field as the
 * result of a failed operation. We do the same.
 */
static void
lx_io_do_op(lx_io_elem_t *ep)
{
	int err;
	int64_t res = 0;

	set_active_fd(ep->lxioelem_fd);

	ttolwp(curthread)->lwp_errno = 0;
	switch (ep->lxioelem_op) {
	case LX_IOCB_CMD_FSYNC:
	case LX_IOCB_CMD_FDSYNC:
		/*
		 * Note that Linux always returns EINVAL for these two
		 * operations. This is apparently because nothing in Linux
		 * defines the 'aio_fsync' function. Thus, it is unlikely any
		 * application will actually submit these.
		 *
		 * This is basically fdsync(), but we already have the fp.
		 */
		err = VOP_FSYNC(ep->lxioelem_fp->f_vnode,
		    (ep->lxioelem_op == LX_IOCB_CMD_FSYNC) ?  FSYNC : FDSYNC,
		    ep->lxioelem_fp->f_cred, NULL);
		if (err != 0) {
			(void) set_errno(err);
		}

		break;

	case LX_IOCB_CMD_PREAD:
		res = lx_pread_fp(ep->lxioelem_fp, ep->lxioelem_buf,
		    ep->lxioelem_nbytes, ep->lxioelem_offset);
		break;

	case LX_IOCB_CMD_PWRITE:
		res = lx_pwrite_fp(ep->lxioelem_fp, ep->lxioelem_buf,
		    ep->lxioelem_nbytes, ep->lxioelem_offset);
		break;

	default:
		/* We validated the op at io_submit syscall time */
		VERIFY(0);
		break;
	}
	if (ttolwp(curthread)->lwp_errno != 0)
		res = -lx_errno(ttolwp(curthread)->lwp_errno, EINVAL);

	ep->lxioelem_res = res;

	releasef(ep->lxioelem_fd);
	ep->lxioelem_fd = 0;
	ep->lxioelem_fp = NULL;
}

/*
 * The operation has either completed or been cancelled. Finalize the handling
 * and move the operation onto the "done" queue.
 */
static void
lx_io_finish_op(lx_io_ctx_t *cp, lx_io_elem_t *ep, boolean_t do_event)
{
	boolean_t do_resfd;
	int resfd = 0;
	file_t *resfp = NULL;

	if (ep->lxioelem_flags & LX_IOCB_FLAG_RESFD) {
		do_resfd = B_TRUE;
		resfd = ep->lxioelem_resfd;
		resfp = ep->lxioelem_resfp;
	} else {
		do_resfd = B_FALSE;
	}

	ep->lxioelem_flags = 0;
	ep->lxioelem_resfd = 0;
	ep->lxioelem_resfp = NULL;

	mutex_enter(&cp->lxioctx_d_lock);
	list_insert_tail(&cp->lxioctx_done, ep);
	cp->lxioctx_done_cnt++;
	cv_signal(&cp->lxioctx_done_cv);
	mutex_exit(&cp->lxioctx_d_lock);

	/* Update the eventfd if necessary */
	if (do_resfd) {
		vnode_t *vp = resfp->f_vnode;
		uint64_t val = 1;

		set_active_fd(resfd);

		if (do_event) {
			/*
			 * Eventfd notifications from AIO are special in that
			 * they are not expected to block. This interface allows
			 * the eventfd value to reach (but not cross) the
			 * overflow value.
			 */
			(void) VOP_IOCTL(vp, EVENTFDIOC_POST, (intptr_t)&val,
			    FKIOCTL, resfp->f_cred, NULL, NULL);
		}

		releasef(resfd);
	}
}

/*
 * First check if this worker needs to quit due to shutdown or exit. Return
 * true in this case.
 *
 * Then check if our process is forking. In this case it expects all LWPs to be
 * stopped first. For the worker threads, a stop equivalent to holdlwp() is
 * necessary before the fork can proceed.
 *
 * It is common to check p_flag outside of p_lock (see issig) and we want to
 * avoid making p_lock any hotter since this is called in the worker main loops.
 */
static boolean_t
lx_io_worker_chk_status(lx_io_ctx_t *cp, boolean_t locked)
{
	if (cp->lxioctx_shutdown)
		return (B_TRUE);

	if (curproc->p_flag & (SEXITLWPS | SKILLED)) {
		cp->lxioctx_shutdown = B_TRUE;
		return (B_TRUE);
	}

	if (curproc->p_flag & (SHOLDFORK | SHOLDFORK1)) {
		if (locked)
			mutex_exit(&cp->lxioctx_p_lock);

		mutex_enter(&curproc->p_lock);
		stop(PR_SUSPENDED, SUSPEND_NORMAL);
		mutex_exit(&curproc->p_lock);

		if (locked)
			mutex_enter(&cp->lxioctx_p_lock);

		if (cp->lxioctx_shutdown)
			return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Worker thread - pull work off the pending queue, perform the operation and
 * place the result on the done queue. Do this as long as work is pending, then
 * wait for more.
 */
static void
lx_io_worker(void *a)
{
	lx_io_ctx_t *cp = (lx_io_ctx_t *)a;
	lx_io_elem_t *ep;

	set_active_fd(-1);	/* See comment in lx_io_cp_rele */

	while (!cp->lxioctx_shutdown) {
		mutex_enter(&cp->lxioctx_p_lock);
		if (list_is_empty(&cp->lxioctx_pending)) {
			/*
			 * This must be cv_wait_sig, as opposed to cv_wait, so
			 * that pokelwps works correctly on these threads.
			 *
			 * The worker threads have all of their signals held,
			 * so a cv_wait_sig return of 0 here only occurs while
			 * we're shutting down.
			 */
			if (cv_wait_sig(&cp->lxioctx_pending_cv,
			    &cp->lxioctx_p_lock) == 0)
				cp->lxioctx_shutdown = B_TRUE;
		}

		if (lx_io_worker_chk_status(cp, B_TRUE)) {
			mutex_exit(&cp->lxioctx_p_lock);
			break;
		}

		ep = list_remove_head(&cp->lxioctx_pending);
		mutex_exit(&cp->lxioctx_p_lock);

		while (ep != NULL) {
			lx_io_do_op(ep);

			lx_io_finish_op(cp, ep, B_TRUE);

			if (lx_io_worker_chk_status(cp, B_FALSE))
				break;

			mutex_enter(&cp->lxioctx_p_lock);
			ep = list_remove_head(&cp->lxioctx_pending);
			mutex_exit(&cp->lxioctx_p_lock);
		}
	}

	lx_io_cp_rele(cp);

	ASSERT(curthread->t_lwp != NULL);
	mutex_enter(&curproc->p_lock);
	lwp_exit();
}

/*
 * LTP passes -1 for nr_events but we're limited by LX_AIO_MAX_NR anyway.
 */
long
lx_io_setup(uint_t nr_events, void *ctxp)
{
	int i, slot;
	proc_t *p = curproc;
	lx_proc_data_t *lxpd = ptolxproc(p);
	lx_zone_data_t *lxzd = ztolxzd(p->p_zone);
	lx_io_ctx_t *cp;
	lx_io_elem_t *ep;
	uintptr_t cid;
	uint_t nworkers;
	k_sigset_t hold_set;

#ifdef _SYSCALL32_IMPL
	if (get_udatamodel() != DATAMODEL_NATIVE) {
		uintptr32_t cid32;

		if (copyin(ctxp, &cid32, sizeof (cid32)) != 0)
			return (set_errno(EFAULT));
		cid = (uintptr_t)cid32;
	} else
#endif
	if (copyin(ctxp, &cid, sizeof (cid)) != 0)
		return (set_errno(EFAULT));

	/* The cid in user-land must be NULL to start */
	if (cid != (uintptr_t)NULL || nr_events > LX_AIO_MAX_NR)
		return (set_errno(EINVAL));

	mutex_enter(&lxzd->lxzd_lock);
	if ((nr_events + lxzd->lxzd_aio_nr) > LX_AIO_MAX_NR) {
		mutex_exit(&lxzd->lxzd_lock);
		return (set_errno(EAGAIN));
	}
	lxzd->lxzd_aio_nr += nr_events;
	mutex_exit(&lxzd->lxzd_lock);

	/* Find a free slot */
	mutex_enter(&lxpd->l_io_ctx_lock);
	if (lxpd->l_io_ctxs == NULL) {
		/*
		 * First use of aio, allocate a context array and a page
		 * in our address space to use for context ID handling.
		 */
		uintptr_t ctxpage;

		ASSERT(lxpd->l_io_ctx_cnt == 0);
		ASSERT(lxpd->l_io_ctxpage == (uintptr_t)NULL);

		ttolwp(curthread)->lwp_errno = 0;
		ctxpage = (uintptr_t)smmap64(0, PAGESIZE, PROT_READ,
		    MAP_SHARED | MAP_ANON, -1, 0);
		if (ttolwp(curthread)->lwp_errno != 0) {
			mutex_exit(&lxpd->l_io_ctx_lock);
			return (set_errno(ENOMEM));
		}

		lxpd->l_io_ctxpage = ctxpage;
		lxpd->l_io_ctx_cnt = LX_IOCTX_CNT_BASE;
		lxpd->l_io_ctxs = kmem_zalloc(lxpd->l_io_ctx_cnt *
		    sizeof (lx_io_ctx_t *), KM_SLEEP);
		slot = 0;
	} else {
		ASSERT(lxpd->l_io_ctx_cnt > 0);
		for (slot = 0; slot < lxpd->l_io_ctx_cnt; slot++) {
			if (lxpd->l_io_ctxs[slot] == NULL)
				break;
		}

		if (slot == lxpd->l_io_ctx_cnt) {
			/* Double our context array up to the max. */
			const uint_t new_cnt = lxpd->l_io_ctx_cnt * 2;
			const uint_t old_size = lxpd->l_io_ctx_cnt *
			    sizeof (lx_io_ctx_t *);
			const uint_t new_size = new_cnt *
			    sizeof (lx_io_ctx_t *);
			struct lx_io_ctx  **old_array = lxpd->l_io_ctxs;

			if (new_cnt > LX_IOCTX_CNT_MAX) {
				mutex_exit(&lxpd->l_io_ctx_lock);
				mutex_enter(&lxzd->lxzd_lock);
				lxzd->lxzd_aio_nr -= nr_events;
				mutex_exit(&lxzd->lxzd_lock);
				return (set_errno(ENOMEM));
			}

			/* See big theory comment explaining context ID. */
			VERIFY(PAGESIZE >= new_size);
			lxpd->l_io_ctxs = kmem_zalloc(new_size, KM_SLEEP);

			bcopy(old_array, lxpd->l_io_ctxs, old_size);
			kmem_free(old_array, old_size);
			lxpd->l_io_ctx_cnt = new_cnt;

			/* note: 'slot' is now valid in the new array */
		}
	}

	cp = kmem_zalloc(sizeof (lx_io_ctx_t), KM_SLEEP);
	list_create(&cp->lxioctx_free, sizeof (lx_io_elem_t),
	    offsetof(lx_io_elem_t, lxioelem_link));
	list_create(&cp->lxioctx_pending, sizeof (lx_io_elem_t),
	    offsetof(lx_io_elem_t, lxioelem_link));
	list_create(&cp->lxioctx_done, sizeof (lx_io_elem_t),
	    offsetof(lx_io_elem_t, lxioelem_link));
	mutex_init(&cp->lxioctx_f_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&cp->lxioctx_p_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&cp->lxioctx_d_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&cp->lxioctx_pending_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&cp->lxioctx_done_cv, NULL, CV_DEFAULT, NULL);

	/* Add a hold on this context until we're done setting up */
	cp->lxioctx_in_use = 1;
	lxpd->l_io_ctxs[slot] = cp;

	cid = CTXID_TO_PTR(lxpd, slot);

	mutex_exit(&lxpd->l_io_ctx_lock);

	/*
	 * Finish setting up the context.
	 *
	 * The context is in the l_io_ctxs array now, so it is potentially
	 * visible to other threads. However, we have a hold so it cannot be
	 * destroyed, and both lxioctx_free_cnt and lxioctx_maxn are still 0,
	 * so nothing can be submitted to this context yet either.
	 */

	/* Setup the free list of internal control block elements */
	for (i = 0; i < nr_events; i++) {
		ep = kmem_zalloc(sizeof (lx_io_elem_t), KM_SLEEP);
		list_insert_head(&cp->lxioctx_free, ep);
	}

	/*
	 * Pre-allocate the worker threads at setup time.
	 *
	 * Based on how much concurrent input we may be given, we want enough
	 * worker threads to get good parallelism but we also want to taper off
	 * and cap at our upper limit. Our zone's ZFS I/O limit may also come
	 * into play when we're pumping lots of I/O in parallel.
	 *
	 * Note: a possible enhancement here would be to also limit the number
	 * of worker threads based on the zone's cpu-cap. That is, if the
	 * cap is low, we might not want too many worker threads.
	 */
	if (nr_events <= lx_aio_base_workers) {
		nworkers = nr_events;
	} else {
		/* scale up until hit max */
		nworkers = (nr_events / 2) + (lx_aio_base_workers / 2);
		if (nworkers > lx_aio_max_workers)
			nworkers = lx_aio_max_workers;
	}

	sigfillset(&hold_set);
	for (i = 0; i < nworkers; i++) {
		klwp_t *l;
		kthread_t *t;

		/*
		 * Note that this lwp will not "stop at sys_rtt" as described
		 * on lwp_create. This lwp will run entirely in the kernel as
		 * a worker thread serving aio requests.
		 */
		l = lwp_create(lx_io_worker, (void *)cp, 0, p, TS_STOPPED,
		    minclsyspri - 1, &hold_set, curthread->t_cid, 0);
		if (l == NULL) {
			if (i == 0) {
				/*
				 * Uh-oh - we can't create a single worker.
				 * Release our hold which will cleanup.
				 */
				cp->lxioctx_shutdown = B_TRUE;
				mutex_enter(&lxpd->l_io_ctx_lock);
				cp->lxioctx_maxn = nr_events;
				mutex_exit(&lxpd->l_io_ctx_lock);
				lx_io_cp_rele(cp);
				return (set_errno(ENOMEM));
			} else {
				/*
				 * No new lwp but we already have at least 1
				 * worker so don't fail entire syscall.
				 */
				break;
			}
		}

		atomic_inc_32(&cp->lxioctx_in_use);

		/*
		 * Mark it as an in-kernel thread, an lx AIO worker LWP, and
		 * set it running.
		 */
		t = lwptot(l);
		mutex_enter(&curproc->p_lock);
		t->t_proc_flag = (t->t_proc_flag & ~TP_HOLDLWP) | TP_KTHREAD;
		lwptolxlwp(l)->br_lwp_flags |= BR_AIO_LWP;
		lwp_create_done(t);
		mutex_exit(&curproc->p_lock);
	}

	/*
	 * io_submit can occur once lxioctx_free_cnt and lxioctx_maxn are
	 * non-zero.
	 */
	mutex_enter(&lxpd->l_io_ctx_lock);
	cp->lxioctx_maxn = cp->lxioctx_free_cnt = nr_events;
	mutex_exit(&lxpd->l_io_ctx_lock);
	/* Release our hold, worker thread refs keep ctx alive. */
	lx_io_cp_rele(cp);

#ifdef _SYSCALL32_IMPL
	if (get_udatamodel() != DATAMODEL_NATIVE) {
		uintptr32_t cid32 = (uintptr32_t)cid;

		if (copyout(&cid32, ctxp, sizeof (cid32)) != 0) {
			/* Since we did a copyin above, this shouldn't fail */
			(void) lx_io_destroy(cid);
			return (set_errno(EFAULT));
		}
	} else
#endif
	if (copyout(&cid, ctxp, sizeof (cid)) != 0) {
		/* Since we did a copyin above, this shouldn't fail */
		(void) lx_io_destroy(cid);
		return (set_errno(EFAULT));
	}

	return (0);
}

long
lx_io_submit(lx_aio_context_t cid, const long nr, uintptr_t **bpp)
{
	uint_t i = 0;
	int err = 0;
	const size_t sz = nr * sizeof (uintptr_t);
	lx_io_ctx_t *cp;
	lx_io_elem_t *ep;
	lx_iocb_t **iocbpp;

	if ((cp = lx_io_cp_hold(cid)) == NULL)
		return (set_errno(EINVAL));

	if (nr == 0) {
		lx_io_cp_rele(cp);
		return (0);
	}

	if (nr < 0 || nr > cp->lxioctx_maxn) {
		lx_io_cp_rele(cp);
		return (set_errno(EINVAL));
	}

	if (nr > MAX_ALLOC_ON_STACK) {
		iocbpp = (lx_iocb_t **)kmem_alloc(sz, KM_NOSLEEP);
		if (iocbpp == NULL) {
			lx_io_cp_rele(cp);
			return (set_errno(EAGAIN));
		}
	} else {
		iocbpp = (lx_iocb_t **)alloca(sz);
	}

#ifdef _SYSCALL32_IMPL
	if (get_udatamodel() != DATAMODEL_NATIVE) {
		uintptr32_t *iocbpp32;

		if (copyin(bpp, iocbpp, nr * sizeof (uintptr32_t)) != 0) {
			lx_io_cp_rele(cp);
			err = EFAULT;
			goto out;
		}

		/*
		 * Zero-extend the 32-bit pointers to proper size.  This is
		 * performed "in reverse" so it can be done in-place, rather
		 * than with an additional translation copy.
		 */
		iocbpp32 = (uintptr32_t *)iocbpp;
		i = nr;
		do {
			i--;
			iocbpp[i] = (lx_iocb_t *)(uintptr_t)iocbpp32[i];
		} while (i != 0);
	} else
#endif
	if (copyin(bpp, iocbpp, nr * sizeof (uintptr_t)) != 0) {
		lx_io_cp_rele(cp);
		err = EFAULT;
		goto out;
	}

	/* We need to return an error if not able to process any of them */
	mutex_enter(&cp->lxioctx_f_lock);
	if (cp->lxioctx_free_cnt == 0) {
		mutex_exit(&cp->lxioctx_f_lock);
		lx_io_cp_rele(cp);
		err = EAGAIN;
		goto out;
	}
	mutex_exit(&cp->lxioctx_f_lock);

	for (i = 0; i < nr; i++) {
		lx_iocb_t cb;
		file_t *fp, *resfp = NULL;

		if (cp->lxioctx_shutdown)
			break;

		if (copyin(iocbpp[i], &cb, sizeof (lx_iocb_t)) != 0) {
			err = EFAULT;
			break;
		}

		/* There is only one valid flag */
		if (cb.lxiocb_flags & ~LX_IOCB_FLAG_RESFD) {
			err = EINVAL;
			break;
		}

		switch (cb.lxiocb_op) {
		case LX_IOCB_CMD_FSYNC:
		case LX_IOCB_CMD_FDSYNC:
		case LX_IOCB_CMD_PREAD:
		case LX_IOCB_CMD_PWRITE:
			break;

		/*
		 * We don't support asynchronous preadv and pwritev (an
		 * asynchronous scatter/gather being a somewhat odd
		 * notion to begin with); we return EINVAL for that
		 * case, which the caller should be able to deal with.
		 * We also return EINVAL for LX_IOCB_CMD_NOOP or any
		 * unrecognized opcode.
		 */
		default:
			err = EINVAL;
			break;
		}
		if (err != 0)
			break;

		/* Validate fd */
		if ((fp = getf(cb.lxiocb_fd)) == NULL) {
			err = EBADF;
			break;
		}

		if (cb.lxiocb_op == LX_IOCB_CMD_PREAD &&
		    (fp->f_flag & FREAD) == 0) {
			err = EBADF;
			releasef(cb.lxiocb_fd);
			break;
		} else if (cb.lxiocb_op == LX_IOCB_CMD_PWRITE &&
		    (fp->f_flag & FWRITE) == 0) {
			err = EBADF;
			releasef(cb.lxiocb_fd);
			break;
		}

		/*
		 * A character device is a bit complicated. Linux seems to
		 * accept these on some devices (e.g. /dev/zero) but not
		 * others (e.g. /proc/self/fd/0). This might be related to
		 * the device being seek-able, but a simple seek-set to the
		 * current offset will succeed for us on a pty. For now we
		 * handle this by rejecting the device if it is a stream.
		 *
		 * If it is a pipe (VFIFO) or directory (VDIR), we error here
		 * as does Linux. If it is a socket (VSOCK), it's ok here but
		 * we will post ESPIPE when processing the I/O CB, as does
		 * Linux. We also error on our other types: VDOOR, VPROC,
		 * VPORT, VBAD.
		 */
		if (fp->f_vnode->v_type == VCHR) {
			if (fp->f_vnode->v_stream != NULL) {
				err = EINVAL;
				releasef(cb.lxiocb_fd);
				break;
			}
		} else if (fp->f_vnode->v_type != VREG &&
		    fp->f_vnode->v_type != VBLK &&
		    fp->f_vnode->v_type != VSOCK) {
			err = EINVAL;
			releasef(cb.lxiocb_fd);
			break;
		}

		if (cb.lxiocb_flags & LX_IOCB_FLAG_RESFD) {
			if ((resfp = getf(cb.lxiocb_resfd)) == NULL ||
			    !lx_is_eventfd(resfp)) {
				err = EINVAL;
				releasef(cb.lxiocb_fd);
				if (resfp != NULL)
					releasef(cb.lxiocb_resfd);
				break;
			}
		}

		mutex_enter(&cp->lxioctx_f_lock);
		if (cp->lxioctx_free_cnt == 0) {
			mutex_exit(&cp->lxioctx_f_lock);
			releasef(cb.lxiocb_fd);
			if (cb.lxiocb_flags & LX_IOCB_FLAG_RESFD) {
				releasef(cb.lxiocb_resfd);
			}
			if (i == 0) {
				/*
				 * Another thread used all of the free entries
				 * after the check preceding this loop. Since
				 * we did nothing, we must return an error.
				 */
				err = EAGAIN;
			}
			break;
		}
		ep = list_remove_head(&cp->lxioctx_free);
		cp->lxioctx_free_cnt--;
		ASSERT(ep != NULL);
		mutex_exit(&cp->lxioctx_f_lock);

		ep->lxioelem_op = cb.lxiocb_op;
		ep->lxioelem_fd = cb.lxiocb_fd;
		ep->lxioelem_fp = fp;
		ep->lxioelem_buf = (void *)(uintptr_t)cb.lxiocb_buf;
		ep->lxioelem_nbytes = cb.lxiocb_nbytes;
		ep->lxioelem_offset = cb.lxiocb_offset;
		ep->lxioelem_data = cb.lxiocb_data;
		ep->lxioelem_cbp = iocbpp[i];

		/* Hang on to the fp but setup to hand it off to a worker */
		clear_active_fd(cb.lxiocb_fd);

		if (cb.lxiocb_flags & LX_IOCB_FLAG_RESFD) {
			ep->lxioelem_flags = LX_IOCB_FLAG_RESFD;
			ep->lxioelem_resfd = cb.lxiocb_resfd;
			ep->lxioelem_resfp = resfp;
			clear_active_fd(cb.lxiocb_resfd);
		}

		mutex_enter(&cp->lxioctx_p_lock);
		list_insert_tail(&cp->lxioctx_pending, ep);
		cv_signal(&cp->lxioctx_pending_cv);
		mutex_exit(&cp->lxioctx_p_lock);
	}

	lx_io_cp_rele(cp);

out:
	if (nr > MAX_ALLOC_ON_STACK) {
		kmem_free(iocbpp, sz);
	}
	if (i == 0 && err != 0)
		return (set_errno(err));

	return (i);
}

long
lx_io_getevents(lx_aio_context_t cid, long min_nr, const long nr,
    lx_io_event_t *events, timespec_t *timeoutp)
{
	int i;
	lx_io_ctx_t *cp;
	const size_t sz = nr * sizeof (lx_io_event_t);
	timespec_t timeout, *tp;
	lx_io_event_t *out;

	if ((cp = lx_io_cp_hold(cid)) == NULL)
		return (set_errno(EINVAL));

	if (min_nr < 0 || min_nr > cp->lxioctx_maxn ||
	    nr < 0 || nr > cp->lxioctx_maxn) {
		lx_io_cp_rele(cp);
		return (set_errno(EINVAL));
	}

	if (nr == 0) {
		lx_io_cp_rele(cp);
		return (0);
	}

	if (events == NULL) {
		lx_io_cp_rele(cp);
		return (set_errno(EFAULT));
	}

	if (timeoutp == NULL) {
		tp = NULL;
	} else {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(timeoutp, &timeout, sizeof (timestruc_t))) {
				lx_io_cp_rele(cp);
				return (EFAULT);
			}
		}
#ifdef _SYSCALL32_IMPL
		else {
			timestruc32_t timeout32;
			if (copyin(timeoutp, &timeout32,
			    sizeof (timestruc32_t))) {
				lx_io_cp_rele(cp);
				return (EFAULT);
			}
			timeout.tv_sec = (time_t)timeout32.tv_sec;
			timeout.tv_nsec = timeout32.tv_nsec;
		}
#endif

		if (itimerspecfix(&timeout)) {
			lx_io_cp_rele(cp);
			return (EINVAL);
		}

		tp = &timeout;
		if (timeout.tv_sec == 0 && timeout.tv_nsec == 0) {
			/*
			 * A timeout of 0:0 is like a poll; we return however
			 * many events are ready, irrespective of the passed
			 * min_nr.
			 */
			min_nr = 0;
		} else {
			timestruc_t now;

			/*
			 * We're given a relative time; add it to the current
			 * time to derive an absolute time.
			 */
			gethrestime(&now);
			timespecadd(tp, &now);
		}
	}

	out = kmem_zalloc(sz, KM_SLEEP);

	/*
	 * A min_nr of 0 is like a poll even if given a NULL timeout; we return
	 * however many events are ready.
	 */
	if (min_nr > 0) {
		mutex_enter(&cp->lxioctx_d_lock);
		while (!cp->lxioctx_shutdown && cp->lxioctx_done_cnt < min_nr) {
			int r;

			r = cv_waituntil_sig(&cp->lxioctx_done_cv,
			    &cp->lxioctx_d_lock, tp, timechanged);
			if (r < 0) {
				/* timeout */
				mutex_exit(&cp->lxioctx_d_lock);
				lx_io_cp_rele(cp);
				kmem_free(out, sz);
				return (0);
			} else if (r == 0) {
				/* interrupted */
				mutex_exit(&cp->lxioctx_d_lock);
				lx_io_cp_rele(cp);
				kmem_free(out, sz);
				return (set_errno(EINTR));
			}

			/*
			 * Signalled that something was queued up. Check if
			 * there are now enough or if we have to wait for more.
			 */
		}
		ASSERT(cp->lxioctx_done_cnt >= min_nr || cp->lxioctx_shutdown);
		mutex_exit(&cp->lxioctx_d_lock);
	}

	/*
	 * For each done control block, move it into the Linux event we return.
	 * As we're doing this, we also moving it from the done list to the
	 * free list.
	 */
	for (i = 0; i < nr && !cp->lxioctx_shutdown; i++) {
		lx_io_event_t *lxe;
		lx_io_elem_t *ep;

		lxe = &out[i];

		mutex_enter(&cp->lxioctx_d_lock);
		if (cp->lxioctx_done_cnt == 0) {
			mutex_exit(&cp->lxioctx_d_lock);
			break;
		}

		ep = list_remove_head(&cp->lxioctx_done);
		cp->lxioctx_done_cnt--;
		mutex_exit(&cp->lxioctx_d_lock);

		lxe->lxioe_data = ep->lxioelem_data;
		lxe->lxioe_object = (uint64_t)(uintptr_t)ep->lxioelem_cbp;
		lxe->lxioe_res = ep->lxioelem_res;
		lxe->lxioe_res2 = 0;

		/* Put it back on the free list */
		ep->lxioelem_cbp = NULL;
		ep->lxioelem_data = 0;
		ep->lxioelem_res = 0;
		mutex_enter(&cp->lxioctx_f_lock);
		list_insert_head(&cp->lxioctx_free, ep);
		cp->lxioctx_free_cnt++;
		mutex_exit(&cp->lxioctx_f_lock);
	}

	lx_io_cp_rele(cp);

	/*
	 * Note: Linux seems to push the events back into the queue if the
	 * copyout fails. Since this error is due to an application bug, it
	 * seems unlikely we need to worry about it, but we can revisit this
	 * if it is ever seen to be an issue.
	 */
	if (i > 0 && copyout(out, events, i * sizeof (lx_io_event_t)) != 0) {
		kmem_free(out, sz);
		return (set_errno(EFAULT));
	}

	kmem_free(out, sz);
	return (i);
}

/*
 * Linux never returns 0 from io_cancel. A successful cancellation will return
 * EINPROGRESS and the result for the cancelled operation will be available via
 * a normal io_getevents call. The third parameter (the "result") to this
 * syscall is unused. Note that currently the Linux man pages are incorrect
 * about this behavior. Also note that in Linux, only the USB driver currently
 * support aio cancellation, so callers will almost always get EINVAL when they
 * attempt to cancel an IO on Linux.
 */
/*ARGSUSED*/
long
lx_io_cancel(lx_aio_context_t cid, lx_iocb_t *iocbp, lx_io_event_t *result)
{
	lx_io_ctx_t *cp;
	lx_io_elem_t *ep;
	uint32_t buf;

	/*
	 * The Linux io_cancel copies in a field from the iocb in order to
	 * locate the matching kernel-internal structure.  To appease the LTP
	 * test case which exercises this, a similar copy is performed here.
	 */
	if (copyin(iocbp, &buf, sizeof (buf)) != 0) {
		return (set_errno(EFAULT));
	}

	if ((cp = lx_io_cp_hold(cid)) == NULL)
		return (set_errno(EINVAL));

	/* Try to pull the CB off the pending list */
	mutex_enter(&cp->lxioctx_p_lock);
	ep = list_head(&cp->lxioctx_pending);
	while (ep != NULL) {
		if (ep->lxioelem_cbp == iocbp) {
			list_remove(&cp->lxioctx_pending, ep);
			break;
		}
		ep = list_next(&cp->lxioctx_pending, ep);
	}
	mutex_exit(&cp->lxioctx_p_lock);

	if (ep == NULL) {
		lx_io_cp_rele(cp);
		return (set_errno(EAGAIN));
	}

	set_active_fd(-1);	 /* See comment in lx_io_cp_rele */
	set_active_fd(ep->lxioelem_fd);
	releasef(ep->lxioelem_fd);
	ep->lxioelem_fd = 0;
	ep->lxioelem_fp = NULL;
	ep->lxioelem_res = -lx_errno(EINTR, EINTR);

	lx_io_finish_op(cp, ep, B_FALSE);
	lx_io_cp_rele(cp);

	return (set_errno(EINPROGRESS));
}

long
lx_io_destroy(lx_aio_context_t cid)
{
	lx_proc_data_t *lxpd = ptolxproc(curproc);
	lx_io_ctx_t *cp;
	int cnt = 0;

	if ((cp = lx_io_cp_hold(cid)) == NULL)
		return (set_errno(EINVAL));

	mutex_enter(&lxpd->l_io_ctx_lock);
	cp->lxioctx_shutdown = B_TRUE;

	/*
	 * Wait for the worker threads and any blocked io_getevents threads to
	 * exit. We have a hold and our rele will cleanup after all other holds
	 * are released.
	 */
	ASSERT(cp->lxioctx_in_use >= 1);
	while (cp->lxioctx_in_use > 1) {
		DTRACE_PROBE2(lx__io__destroy, lx_io_ctx_t *, cp, int, cnt);
		cv_broadcast(&cp->lxioctx_pending_cv);
		cv_broadcast(&cp->lxioctx_done_cv);

		/*
		 * Each worker has a hold. We want to let those threads finish
		 * up and exit.
		 */
		cv_wait(&lxpd->l_io_destroy_cv, &lxpd->l_io_ctx_lock);
		cnt++;
	}

	mutex_exit(&lxpd->l_io_ctx_lock);
	lx_io_cp_rele(cp);
	return (0);
}

/*
 * Called at proc fork to clear contexts from child. We don't bother to unmap
 * l_io_ctxpage since the vast majority of processes will immediately exec and
 * cause an unmapping. If the child does not exec, there will simply be a
 * single shared page in its address space, so no additional anonymous memory
 * is consumed.
 */
void
lx_io_clear(lx_proc_data_t *cpd)
{
	cpd->l_io_ctxs = NULL;
	cpd->l_io_ctx_cnt = 0;
	cpd->l_io_ctxpage = (uintptr_t)NULL;
}

/*
 * Called via lx_proc_exit to cleanup any existing io context array. All
 * worker threads should have already exited by this point, so all contexts
 * should already be deleted.
 */
void
lx_io_cleanup(proc_t *p)
{
	lx_proc_data_t *lxpd;
	int i;

	mutex_enter(&p->p_lock);
	VERIFY((lxpd = ptolxproc(p)) != NULL);
	mutex_exit(&p->p_lock);

	mutex_enter(&lxpd->l_io_ctx_lock);
	if (lxpd->l_io_ctxs == NULL) {
		ASSERT(lxpd->l_io_ctx_cnt == 0);
		mutex_exit(&lxpd->l_io_ctx_lock);
		return;
	}

	ASSERT(lxpd->l_io_ctx_cnt > 0);
	for (i = 0; i < lxpd->l_io_ctx_cnt; i++) {
		ASSERT(lxpd->l_io_ctxs[i] == NULL);
	}

	kmem_free(lxpd->l_io_ctxs, lxpd->l_io_ctx_cnt * sizeof (lx_io_ctx_t *));
	lxpd->l_io_ctxs = NULL;
	lxpd->l_io_ctx_cnt = 0;
	mutex_exit(&lxpd->l_io_ctx_lock);
}
