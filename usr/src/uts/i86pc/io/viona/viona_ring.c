/*
 * Copyright (c) 2013  Chris Torek <torek @ torek net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * Copyright 2015 Pluribus Networks Inc.
 * Copyright 2019 Joyent, Inc.
 * Copyright 2021 Oxide Computer Company
 */


#include <sys/disp.h>

#include "viona_impl.h"

#define	VRING_MAX_LEN		32768

/* Layout and sizing as defined in the spec for a legacy-style virtqueue */

#define	LEGACY_VQ_ALIGN		PAGESIZE

#define	LEGACY_DESC_SZ(qsz)	((qsz) * sizeof (struct virtio_desc))
/*
 * Available ring consists of avail_idx (uint16_t), flags (uint16_t), qsz avail
 * descriptors (uint16_t each), and (optional) used_event (uint16_t).
 */
#define	LEGACY_AVAIL_SZ(qsz)	(((qsz) + 3) * sizeof (uint16_t))
/*
 * Used ring consists of used_idx (uint16_t), flags (uint16_t), qsz used
 * descriptors (two uint32_t each), and (optional) avail_event (uint16_t).
 */
#define	LEGACY_USED_SZ(qsz)	\
	((qsz) * sizeof (struct virtio_used) + 3 * sizeof (uint16_t))

#define	LEGACY_AVAIL_FLAGS_OFF(qsz)	LEGACY_DESC_SZ(qsz)
#define	LEGACY_AVAIL_IDX_OFF(qsz)	\
	(LEGACY_DESC_SZ(qsz) + sizeof (uint16_t))
#define	LEGACY_AVAIL_ENT_OFF(qsz, idx)	\
	(LEGACY_DESC_SZ(qsz) + (2 + (idx)) * sizeof (uint16_t))

#define	LEGACY_USED_FLAGS_OFF(qsz)	\
	P2ROUNDUP(LEGACY_DESC_SZ(qsz) + LEGACY_AVAIL_SZ(qsz), LEGACY_VQ_ALIGN)
#define	LEGACY_USED_IDX_OFF(qsz)	\
	(LEGACY_USED_FLAGS_OFF(qsz) + sizeof (uint16_t))
#define	LEGACY_USED_ENT_OFF(qsz, idx)	\
	(LEGACY_USED_FLAGS_OFF(qsz) + 2 * sizeof (uint16_t) + \
	(idx) * sizeof (struct virtio_used))

#define	LEGACY_VQ_SIZE(qsz)	\
	(LEGACY_USED_FLAGS_OFF(qsz) + \
	P2ROUNDUP(LEGACY_USED_SZ(qsz), LEGACY_VQ_ALIGN))
#define	LEGACY_VQ_PAGES(qsz)	(LEGACY_VQ_SIZE(qsz) / PAGESIZE)

static boolean_t viona_ring_map(viona_vring_t *);
static void viona_ring_unmap(viona_vring_t *);
static kthread_t *viona_create_worker(viona_vring_t *);

static void *
viona_hold_page(viona_vring_t *ring, uint64_t gpa)
{
	ASSERT3P(ring->vr_lease, !=, NULL);
	ASSERT3U(gpa & PAGEOFFSET, ==, 0);

	return (vmm_drv_gpa2kva(ring->vr_lease, gpa, PAGESIZE));
}

static boolean_t
viona_ring_lease_expire_cb(void *arg)
{
	viona_vring_t *ring = arg;

	cv_broadcast(&ring->vr_cv);

	/* The lease will be broken asynchronously. */
	return (B_FALSE);
}

static void
viona_ring_lease_drop(viona_vring_t *ring)
{
	ASSERT(MUTEX_HELD(&ring->vr_lock));

	if (ring->vr_lease != NULL) {
		vmm_hold_t *hold = ring->vr_link->l_vm_hold;

		ASSERT(hold != NULL);

		/*
		 * Without an active lease, the ring mappings cannot be
		 * considered valid.
		 */
		viona_ring_unmap(ring);

		vmm_drv_lease_break(hold, ring->vr_lease);
		ring->vr_lease = NULL;
	}
}

boolean_t
viona_ring_lease_renew(viona_vring_t *ring)
{
	vmm_hold_t *hold = ring->vr_link->l_vm_hold;

	ASSERT(hold != NULL);
	ASSERT(MUTEX_HELD(&ring->vr_lock));

	viona_ring_lease_drop(ring);

	/*
	 * Lease renewal will fail if the VM has requested that all holds be
	 * cleaned up.
	 */
	ring->vr_lease = vmm_drv_lease_sign(hold, viona_ring_lease_expire_cb,
	    ring);
	if (ring->vr_lease != NULL) {
		/* A ring undergoing renewal will need valid guest mappings */
		if (ring->vr_pa != 0 && ring->vr_size != 0) {
			/*
			 * If new mappings cannot be established, consider the
			 * lease renewal a failure.
			 */
			if (!viona_ring_map(ring)) {
				viona_ring_lease_drop(ring);
				return (B_FALSE);
			}
		}
	}
	return (ring->vr_lease != NULL);
}

void
viona_ring_alloc(viona_link_t *link, viona_vring_t *ring)
{
	ring->vr_link = link;
	mutex_init(&ring->vr_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&ring->vr_cv, NULL, CV_DRIVER, NULL);
	mutex_init(&ring->vr_a_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&ring->vr_u_mutex, NULL, MUTEX_DRIVER, NULL);
}

static void
viona_ring_misc_free(viona_vring_t *ring)
{
	const uint_t qsz = ring->vr_size;

	viona_tx_ring_free(ring, qsz);
}

void
viona_ring_free(viona_vring_t *ring)
{
	mutex_destroy(&ring->vr_lock);
	cv_destroy(&ring->vr_cv);
	mutex_destroy(&ring->vr_a_mutex);
	mutex_destroy(&ring->vr_u_mutex);
	ring->vr_link = NULL;
}

int
viona_ring_init(viona_link_t *link, uint16_t idx, uint16_t qsz, uint64_t pa)
{
	viona_vring_t *ring;
	kthread_t *t;
	int err = 0;

	if (idx >= VIONA_VQ_MAX) {
		return (EINVAL);
	}
	if (qsz == 0 || qsz > VRING_MAX_LEN || (1 << (ffs(qsz) - 1)) != qsz) {
		return (EINVAL);
	}
	if ((pa & (LEGACY_VQ_ALIGN - 1)) != 0) {
		return (EINVAL);
	}

	ring = &link->l_vrings[idx];
	mutex_enter(&ring->vr_lock);
	if (ring->vr_state != VRS_RESET) {
		mutex_exit(&ring->vr_lock);
		return (EBUSY);
	}
	VERIFY(ring->vr_state_flags == 0);

	ring->vr_lease = NULL;
	if (!viona_ring_lease_renew(ring)) {
		err = EBUSY;
		goto fail;
	}

	ring->vr_size = qsz;
	ring->vr_mask = (ring->vr_size - 1);
	ring->vr_pa = pa;
	if (!viona_ring_map(ring)) {
		err = EINVAL;
		goto fail;
	}

	/* Initialize queue indexes */
	ring->vr_cur_aidx = 0;
	ring->vr_cur_uidx = 0;

	if (idx == VIONA_VQ_TX) {
		viona_tx_ring_alloc(ring, qsz);
	}

	/* Zero out MSI-X configuration */
	ring->vr_msi_addr = 0;
	ring->vr_msi_msg = 0;

	/* Clear the stats */
	bzero(&ring->vr_stats, sizeof (ring->vr_stats));

	t = viona_create_worker(ring);
	if (t == NULL) {
		err = ENOMEM;
		goto fail;
	}
	ring->vr_worker_thread = t;
	ring->vr_state = VRS_SETUP;
	cv_broadcast(&ring->vr_cv);
	mutex_exit(&ring->vr_lock);
	return (0);

fail:
	viona_ring_lease_drop(ring);
	viona_ring_misc_free(ring);
	ring->vr_size = 0;
	ring->vr_mask = 0;
	ring->vr_pa = 0;
	mutex_exit(&ring->vr_lock);
	return (err);
}

int
viona_ring_reset(viona_vring_t *ring, boolean_t heed_signals)
{
	mutex_enter(&ring->vr_lock);
	if (ring->vr_state == VRS_RESET) {
		mutex_exit(&ring->vr_lock);
		return (0);
	}

	if ((ring->vr_state_flags & VRSF_REQ_STOP) == 0) {
		ring->vr_state_flags |= VRSF_REQ_STOP;
		cv_broadcast(&ring->vr_cv);
	}
	while (ring->vr_state != VRS_RESET) {
		if (!heed_signals) {
			cv_wait(&ring->vr_cv, &ring->vr_lock);
		} else {
			int rs;

			rs = cv_wait_sig(&ring->vr_cv, &ring->vr_lock);
			if (rs <= 0 && ring->vr_state != VRS_RESET) {
				mutex_exit(&ring->vr_lock);
				return (EINTR);
			}
		}
	}
	mutex_exit(&ring->vr_lock);
	return (0);
}

static boolean_t
viona_ring_map(viona_vring_t *ring)
{
	const uint16_t qsz = ring->vr_size;
	uintptr_t pa = ring->vr_pa;

	ASSERT3U(qsz, !=, 0);
	ASSERT3U(qsz, <=, VRING_MAX_LEN);
	ASSERT3U(pa, !=, 0);
	ASSERT3U(pa & (LEGACY_VQ_ALIGN - 1), ==, 0);
	ASSERT3U(LEGACY_VQ_ALIGN, ==, PAGESIZE);
	ASSERT(MUTEX_HELD(&ring->vr_lock));
	ASSERT3P(ring->vr_map_pages, ==, NULL);

	const uint_t npages = LEGACY_VQ_PAGES(qsz);
	ring->vr_map_pages = kmem_zalloc(npages * sizeof (void *), KM_SLEEP);

	for (uint_t i = 0; i < npages; i++, pa += PAGESIZE) {
		void *page = viona_hold_page(ring, pa);

		if (page == NULL) {
			viona_ring_unmap(ring);
			return (B_FALSE);
		}
		ring->vr_map_pages[i] = page;
	}

	return (B_TRUE);
}

static void
viona_ring_unmap(viona_vring_t *ring)
{
	ASSERT(MUTEX_HELD(&ring->vr_lock));

	void **map = ring->vr_map_pages;
	if (map != NULL) {
		/*
		 * The bhyve page-hold mechanism does not currently require a
		 * corresponding page-release action, given the simplicity of
		 * the underlying virtual memory constructs.
		 *
		 * If/when those systems become more sophisticated, more than a
		 * simple free of the page pointers will be required here.
		 */
		const uint_t npages = LEGACY_VQ_PAGES(ring->vr_size);
		kmem_free(map, npages * sizeof (void *));
		ring->vr_map_pages = NULL;
	}
}

static inline void *
viona_ring_addr(viona_vring_t *ring, uint_t off)
{
	ASSERT3P(ring->vr_map_pages, !=, NULL);
	ASSERT3U(LEGACY_VQ_SIZE(ring->vr_size), >, off);

	const uint_t page_num = off / PAGESIZE;
	const uint_t page_off = off % PAGESIZE;
	return ((caddr_t)ring->vr_map_pages[page_num] + page_off);
}

void
viona_intr_ring(viona_vring_t *ring, boolean_t skip_flags_check)
{
	if (!skip_flags_check) {
		volatile uint16_t *avail_flags = viona_ring_addr(ring,
		    LEGACY_AVAIL_FLAGS_OFF(ring->vr_size));

		if ((*avail_flags & VRING_AVAIL_F_NO_INTERRUPT) != 0) {
			return;
		}
	}

	mutex_enter(&ring->vr_lock);
	uint64_t addr = ring->vr_msi_addr;
	uint64_t msg = ring->vr_msi_msg;
	mutex_exit(&ring->vr_lock);
	if (addr != 0) {
		/* Deliver the interrupt directly, if so configured... */
		(void) vmm_drv_msi(ring->vr_lease, addr, msg);
	} else {
		/* ... otherwise, leave it to userspace */
		if (atomic_cas_uint(&ring->vr_intr_enabled, 0, 1) == 0) {
			pollwakeup(&ring->vr_link->l_pollhead, POLLRDBAND);
		}
	}
}

static void
viona_worker(void *arg)
{
	viona_vring_t *ring = (viona_vring_t *)arg;
	viona_link_t *link = ring->vr_link;
	proc_t *p = ttoproc(curthread);

	mutex_enter(&ring->vr_lock);
	VERIFY3U(ring->vr_state, ==, VRS_SETUP);

	/* Bail immediately if ring shutdown or process exit was requested */
	if (VRING_NEED_BAIL(ring, p)) {
		goto cleanup;
	}

	/* Report worker thread as alive and notify creator */
	ring->vr_state = VRS_INIT;
	cv_broadcast(&ring->vr_cv);

	while (ring->vr_state_flags == 0) {
		/*
		 * Keeping lease renewals timely while waiting for the ring to
		 * be started is important for avoiding deadlocks.
		 */
		if (vmm_drv_lease_expired(ring->vr_lease)) {
			if (!viona_ring_lease_renew(ring)) {
				goto cleanup;
			}
		}

		(void) cv_wait_sig(&ring->vr_cv, &ring->vr_lock);

		if (VRING_NEED_BAIL(ring, p)) {
			goto cleanup;
		}
	}

	ASSERT((ring->vr_state_flags & VRSF_REQ_START) != 0);
	ring->vr_state = VRS_RUN;
	ring->vr_state_flags &= ~VRSF_REQ_START;

	/* Ensure ring lease is valid first */
	if (vmm_drv_lease_expired(ring->vr_lease)) {
		if (!viona_ring_lease_renew(ring)) {
			goto cleanup;
		}
	}

	/* Process actual work */
	if (ring == &link->l_vrings[VIONA_VQ_RX]) {
		viona_worker_rx(ring, link);
	} else if (ring == &link->l_vrings[VIONA_VQ_TX]) {
		viona_worker_tx(ring, link);
	} else {
		panic("unexpected ring: %p", (void *)ring);
	}

	VERIFY3U(ring->vr_state, ==, VRS_STOP);

cleanup:
	if (ring->vr_txdesb != NULL) {
		/*
		 * Transmit activity must be entirely concluded before the
		 * associated descriptors can be cleaned up.
		 */
		VERIFY(ring->vr_xfer_outstanding == 0);
	}
	viona_ring_misc_free(ring);

	viona_ring_lease_drop(ring);
	ring->vr_cur_aidx = 0;
	ring->vr_size = 0;
	ring->vr_mask = 0;
	ring->vr_pa = 0;
	ring->vr_state = VRS_RESET;
	ring->vr_state_flags = 0;
	ring->vr_worker_thread = NULL;
	cv_broadcast(&ring->vr_cv);
	mutex_exit(&ring->vr_lock);

	mutex_enter(&ttoproc(curthread)->p_lock);
	lwp_exit();
}

static kthread_t *
viona_create_worker(viona_vring_t *ring)
{
	k_sigset_t hold_set;
	proc_t *p = curproc;
	kthread_t *t;
	klwp_t *lwp;

	ASSERT(MUTEX_HELD(&ring->vr_lock));
	ASSERT(ring->vr_state == VRS_RESET);

	sigfillset(&hold_set);
	lwp = lwp_create(viona_worker, (void *)ring, 0, p, TS_STOPPED,
	    minclsyspri - 1, &hold_set, curthread->t_cid, 0);
	if (lwp == NULL) {
		return (NULL);
	}

	t = lwptot(lwp);
	mutex_enter(&p->p_lock);
	t->t_proc_flag = (t->t_proc_flag & ~TP_HOLDLWP) | TP_KTHREAD;
	lwp_create_done(t);
	mutex_exit(&p->p_lock);

	return (t);
}

void
vq_read_desc(viona_vring_t *ring, uint16_t idx, struct virtio_desc *descp)
{
	const uint_t entry_off = idx * sizeof (struct virtio_desc);

	ASSERT3U(idx, <, ring->vr_size);

	bcopy(viona_ring_addr(ring, entry_off), descp, sizeof (*descp));
}

static uint16_t
vq_read_avail(viona_vring_t *ring, uint16_t idx)
{
	ASSERT3U(idx, <, ring->vr_size);

	volatile uint16_t *avail_ent =
	    viona_ring_addr(ring, LEGACY_AVAIL_ENT_OFF(ring->vr_size, idx));
	return (*avail_ent);
}

/*
 * Given a buffer descriptor `desc`, attempt to map the pages backing that
 * region of guest physical memory, taking into account that there are no
 * guarantees about guest-contiguous pages being host-contiguous.
 */
static int
vq_map_desc_bufs(viona_vring_t *ring, const struct virtio_desc *desc,
    struct iovec *iov, uint_t niov, uint16_t *idxp)
{
	uint64_t gpa = desc->vd_addr;
	uint32_t len = desc->vd_len;
	uint16_t lidx = *idxp;
	caddr_t buf;

	ASSERT3U(lidx, <, niov);

	if (desc->vd_len == 0) {
		VIONA_PROBE2(desc_bad_len, viona_vring_t *, ring,
		    uint32_t, desc->vd_len);
		VIONA_RING_STAT_INCR(ring, desc_bad_len);
		return (EINVAL);
	}

	const uint32_t front_offset = desc->vd_addr & PAGEOFFSET;
	const uint32_t front_len = MIN(len, PAGESIZE - front_offset);
	uint_t pages = 1;
	if (front_len < len) {
		pages += P2ROUNDUP((uint64_t)(len - front_len),
		    PAGESIZE) / PAGESIZE;
	}

	if (pages > (niov - lidx)) {
		VIONA_PROBE1(too_many_desc, viona_vring_t *, ring);
		VIONA_RING_STAT_INCR(ring, too_many_desc);
		return (E2BIG);
	}

	buf = viona_hold_page(ring, gpa & PAGEMASK);
	if (buf == NULL) {
		VIONA_PROBE_BAD_RING_ADDR(ring, desc->vd_addr);
		VIONA_RING_STAT_INCR(ring, bad_ring_addr);
		return (EFAULT);
	}
	iov[lidx].iov_base = buf + front_offset;
	iov[lidx].iov_len = front_len;
	gpa += front_len;
	len -= front_len;
	lidx++;

	for (uint_t i = 1; i < pages; i++) {
		ASSERT3U(gpa & PAGEOFFSET, ==, 0);

		buf = viona_hold_page(ring, gpa);
		if (buf == NULL) {
			VIONA_PROBE_BAD_RING_ADDR(ring, desc->vd_addr);
			VIONA_RING_STAT_INCR(ring, bad_ring_addr);
			return (EFAULT);
		}

		const uint32_t region_len = MIN(len, PAGESIZE);
		iov[lidx].iov_base = buf;
		iov[lidx].iov_len = region_len;
		gpa += region_len;
		len -= region_len;
		lidx++;
	}

	ASSERT3U(len, ==, 0);
	ASSERT3U(gpa, ==, desc->vd_addr + desc->vd_len);

	*idxp = lidx;
	return (0);
}

/*
 * Walk an indirect buffer descriptor `desc`, attempting to map the pages
 * backing the regions of guest memory covered by its contituent descriptors.
 */
static int
vq_map_indir_desc_bufs(viona_vring_t *ring, const struct virtio_desc *desc,
    struct iovec *iov, uint_t niov, uint16_t *idxp)
{
	const uint16_t indir_count = desc->vd_len / sizeof (struct virtio_desc);

	if ((desc->vd_len & 0xf) != 0 || indir_count == 0 ||
	    indir_count > ring->vr_size ||
	    desc->vd_addr > (desc->vd_addr + desc->vd_len)) {
		VIONA_PROBE2(indir_bad_len, viona_vring_t *, ring,
		    uint32_t, desc->vd_len);
		VIONA_RING_STAT_INCR(ring, indir_bad_len);
		return (EINVAL);
	}

	uint16_t indir_next = 0;
	caddr_t buf = NULL;
	uint64_t buf_gpa = UINT64_MAX;

	for (;;) {
		uint64_t indir_gpa =
		    desc->vd_addr + (indir_next * sizeof (struct virtio_desc));
		uint64_t indir_page = indir_gpa & PAGEMASK;
		struct virtio_desc vp;

		/*
		 * Get a mapping for the page that the next indirect descriptor
		 * resides in, if has not already been done.
		 */
		if (indir_page != buf_gpa) {
			buf = viona_hold_page(ring, indir_page);
			if (buf == NULL) {
				VIONA_PROBE_BAD_RING_ADDR(ring, desc->vd_addr);
				VIONA_RING_STAT_INCR(ring, bad_ring_addr);
				return (EFAULT);
			}
			buf_gpa = indir_page;
		}

		/*
		 * A copy of the indirect descriptor is made here, rather than
		 * simply using a reference pointer.  This prevents malicious or
		 * erroneous guest writes to the descriptor from fooling the
		 * flags/bounds verification through a race.
		 */
		bcopy(buf + (indir_gpa - indir_page), &vp, sizeof (vp));

		if (vp.vd_flags & VRING_DESC_F_INDIRECT) {
			VIONA_PROBE1(indir_bad_nest, viona_vring_t *, ring);
			VIONA_RING_STAT_INCR(ring, indir_bad_nest);
			return (EINVAL);
		} else if (vp.vd_len == 0) {
			VIONA_PROBE2(desc_bad_len, viona_vring_t *, ring,
			    uint32_t, vp.vd_len);
			VIONA_RING_STAT_INCR(ring, desc_bad_len);
			return (EINVAL);
		}

		int err = vq_map_desc_bufs(ring, &vp, iov, niov, idxp);
		if (err != 0) {
			return (err);
		}

		/* Successfully reach the end of the indir chain */
		if ((vp.vd_flags & VRING_DESC_F_NEXT) == 0) {
			return (0);
		}
		if (*idxp >= niov) {
			VIONA_PROBE1(too_many_desc, viona_vring_t *, ring);
			VIONA_RING_STAT_INCR(ring, too_many_desc);
			return (E2BIG);
		}

		indir_next = vp.vd_next;
		if (indir_next >= indir_count) {
			VIONA_PROBE3(indir_bad_next, viona_vring_t *, ring,
			    uint16_t, indir_next, uint16_t, indir_count);
			VIONA_RING_STAT_INCR(ring, indir_bad_next);
			return (EINVAL);
		}
	}

	/* NOTREACHED */
	return (-1);
}

int
vq_popchain(viona_vring_t *ring, struct iovec *iov, uint_t niov,
    uint16_t *cookie)
{
	uint16_t i, ndesc, idx, head, next;
	struct virtio_desc vdir;

	ASSERT(iov != NULL);
	ASSERT(niov > 0 && niov < INT_MAX);

	mutex_enter(&ring->vr_a_mutex);
	idx = ring->vr_cur_aidx;
	ndesc = viona_ring_num_avail(ring);

	if (ndesc == 0) {
		mutex_exit(&ring->vr_a_mutex);
		return (0);
	}
	if (ndesc > ring->vr_size) {
		/*
		 * Despite the fact that the guest has provided an 'avail_idx'
		 * which indicates that an impossible number of descriptors are
		 * available, continue on and attempt to process the next one.
		 *
		 * The transgression will not escape the probe or stats though.
		 */
		VIONA_PROBE2(ndesc_too_high, viona_vring_t *, ring,
		    uint16_t, ndesc);
		VIONA_RING_STAT_INCR(ring, ndesc_too_high);
	}

	head = vq_read_avail(ring, idx & ring->vr_mask);
	next = head;

	for (i = 0; i < niov; next = vdir.vd_next) {
		if (next >= ring->vr_size) {
			VIONA_PROBE2(bad_idx, viona_vring_t *, ring,
			    uint16_t, next);
			VIONA_RING_STAT_INCR(ring, bad_idx);
			break;
		}

		vq_read_desc(ring, next, &vdir);
		if ((vdir.vd_flags & VRING_DESC_F_INDIRECT) == 0) {
			if (vq_map_desc_bufs(ring, &vdir, iov, niov, &i) != 0) {
				break;
			}
		} else {
			/*
			 * Per the specification (Virtio 1.1 S2.6.5.3.1):
			 *   A driver MUST NOT set both VIRTQ_DESC_F_INDIRECT
			 *   and VIRTQ_DESC_F_NEXT in `flags`.
			 */
			if ((vdir.vd_flags & VRING_DESC_F_NEXT) != 0) {
				VIONA_PROBE3(indir_bad_next,
				    viona_vring_t *, ring,
				    uint16_t, next, uint16_t, 0);
				VIONA_RING_STAT_INCR(ring, indir_bad_next);
				break;
			}

			if (vq_map_indir_desc_bufs(ring, &vdir, iov, niov, &i)
			    != 0) {
				break;
			}
		}

		if ((vdir.vd_flags & VRING_DESC_F_NEXT) == 0) {
			*cookie = head;
			ring->vr_cur_aidx++;
			mutex_exit(&ring->vr_a_mutex);
			return (i);
		}
	}

	mutex_exit(&ring->vr_a_mutex);
	return (-1);
}


static void
vq_write_used_ent(viona_vring_t *ring, uint16_t idx, uint16_t cookie,
    uint32_t len)
{
	/*
	 * In a larger ring, entry could be split across pages, so be sure to
	 * account for that when configuring the transfer by looking up the ID
	 * and length addresses separately, rather than an address for a
	 * combined `struct virtio_used`.
	 */
	const uint_t used_id_off = LEGACY_USED_ENT_OFF(ring->vr_size, idx);
	const uint_t used_len_off = used_id_off + sizeof (uint32_t);
	volatile uint32_t *idp = viona_ring_addr(ring, used_id_off);
	volatile uint32_t *lenp = viona_ring_addr(ring, used_len_off);

	ASSERT(MUTEX_HELD(&ring->vr_u_mutex));

	*idp = cookie;
	*lenp = len;
}

static void
vq_write_used_idx(viona_vring_t *ring, uint16_t idx)
{
	ASSERT(MUTEX_HELD(&ring->vr_u_mutex));

	volatile uint16_t *used_idx =
	    viona_ring_addr(ring, LEGACY_USED_IDX_OFF(ring->vr_size));
	*used_idx = idx;
}

void
vq_pushchain(viona_vring_t *ring, uint32_t len, uint16_t cookie)
{
	uint16_t uidx;

	mutex_enter(&ring->vr_u_mutex);

	uidx = ring->vr_cur_uidx;
	vq_write_used_ent(ring, uidx & ring->vr_mask, cookie, len);
	uidx++;
	membar_producer();

	vq_write_used_idx(ring, uidx);
	ring->vr_cur_uidx = uidx;

	mutex_exit(&ring->vr_u_mutex);
}

void
vq_pushchain_many(viona_vring_t *ring, uint_t num_bufs, used_elem_t *elem)
{
	uint16_t uidx;

	mutex_enter(&ring->vr_u_mutex);

	uidx = ring->vr_cur_uidx;

	for (uint_t i = 0; i < num_bufs; i++) {
		vq_write_used_ent(ring, uidx & ring->vr_mask, elem[i].id,
		    elem[i].len);
	}
	uidx += num_bufs;

	membar_producer();
	vq_write_used_idx(ring, uidx);
	ring->vr_cur_uidx = uidx;

	mutex_exit(&ring->vr_u_mutex);
}

/*
 * Set USED_NO_NOTIFY on VQ so guest elides doorbell calls for new entries.
 */
void
viona_ring_disable_notify(viona_vring_t *ring)
{
	volatile uint16_t *used_flags =
	    viona_ring_addr(ring, LEGACY_USED_FLAGS_OFF(ring->vr_size));

	*used_flags |= VRING_USED_F_NO_NOTIFY;
}

/*
 * Clear USED_NO_NOTIFY on VQ so guest resumes doorbell calls for new entries.
 */
void
viona_ring_enable_notify(viona_vring_t *ring)
{
	volatile uint16_t *used_flags =
	    viona_ring_addr(ring, LEGACY_USED_FLAGS_OFF(ring->vr_size));

	*used_flags &= ~VRING_USED_F_NO_NOTIFY;
}

/*
 * Return the number of available descriptors in the vring taking care of the
 * 16-bit index wraparound.
 *
 * Note: If the number of apparently available descriptors is larger than the
 * ring size (due to guest misbehavior), this check will still report the
 * positive count of descriptors.
 */
uint16_t
viona_ring_num_avail(viona_vring_t *ring)
{
	volatile uint16_t *avail_idx =
	    viona_ring_addr(ring, LEGACY_AVAIL_IDX_OFF(ring->vr_size));

	return (*avail_idx - ring->vr_cur_aidx);
}
