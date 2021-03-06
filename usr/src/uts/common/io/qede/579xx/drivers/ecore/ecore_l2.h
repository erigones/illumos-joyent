/*
* CDDL HEADER START
*
* The contents of this file are subject to the terms of the
* Common Development and Distribution License, v.1,  (the "License").
* You may not use this file except in compliance with the License.
*
* You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
* or http://opensource.org/licenses/CDDL-1.0.
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
* Copyright 2014-2017 Cavium, Inc. 
* The contents of this file are subject to the terms of the Common Development 
* and Distribution License, v.1,  (the "License").

* You may not use this file except in compliance with the License.

* You can obtain a copy of the License at available 
* at http://opensource.org/licenses/CDDL-1.0

* See the License for the specific language governing permissions and 
* limitations under the License.
*/

#ifndef __ECORE_L2_H__
#define __ECORE_L2_H__


#include "ecore.h"
#include "ecore_hw.h"
#include "ecore_spq.h"
#include "ecore_l2_api.h"

#define MAX_QUEUES_PER_QZONE	(sizeof(unsigned long) * 8)
#define ECORE_QUEUE_CID_PF	(0xff)

/* Almost identical to the ecore_queue_start_common_params,
 * but here we maintain the SB index in IGU CAM.
 */
struct ecore_queue_cid_params {
	u8 vport_id;
	u16 queue_id;
	u8 stats_id;
};

 /* Additional parameters required for initialization of the queue_cid
 * and are relevant only for a PF initializing one for its VFs.
 */
struct ecore_queue_cid_vf_params {
	/* Should match the VF's relative index */
	u8 vfid;

	/* 0-based queue index. Should reflect the relative qzone the
	 * VF thinks is associated with it [in its range].
	 */
	u8 vf_qid;

	/* Indicates a VF is legacy, making it differ in several things:
	 *  - Producers would be placed in a different place.
	 *  - Makes assumptions regarding the CIDs.
	 */
	u8 vf_legacy;

	/* For VFs, this index arrives via TLV to diffrentiate between
	 * different queues opened on the same qzone, and is passed
	 * [where the PF would have allocated it internally for its own].
	 */
	u8 qid_usage_idx;
};

struct ecore_queue_cid {
	/* For stats-id, the `rel' is actually absolute as well */
	struct ecore_queue_cid_params rel;
	struct ecore_queue_cid_params abs;

	/* These have no 'relative' meaning */
	u16 sb_igu_id;
	u8 sb_idx;

	u32 cid;
	u16 opaque_fid;

	/* VFs queues are mapped differently, so we need to know the
	 * relative queue associated with them [0-based].
	 * Notice this is relevant on the *PF* queue-cid of its VF's queues,
	 * and not on the VF itself.
	 */
	u8 vfid;
	u8 vf_qid;

	/* We need an additional index to diffrentiate between queues opened
	 * for same queue-zone, as VFs would have to communicate the info
	 * to the PF [otherwise PF has no way to diffrentiate].
	 */
	u8 qid_usage_idx;

	/* Legacy VFs might have Rx producer located elsewhere */
	u8 vf_legacy;
#define ECORE_QCID_LEGACY_VF_RX_PROD	(1 << 0)
#define ECORE_QCID_LEGACY_VF_CID	(1 << 1)

	struct ecore_hwfn *p_owner;
};

enum _ecore_status_t ecore_l2_alloc(struct ecore_hwfn *p_hwfn);
void ecore_l2_setup(struct ecore_hwfn *p_hwfn);
void ecore_l2_free(struct ecore_hwfn *p_hwfn);

void ecore_eth_queue_cid_release(struct ecore_hwfn *p_hwfn,
				 struct ecore_queue_cid *p_cid);

struct ecore_queue_cid *
ecore_eth_queue_to_cid(struct ecore_hwfn *p_hwfn, u16 opaque_fid,
		       struct ecore_queue_start_common_params *p_params,
		       struct ecore_queue_cid_vf_params *p_vf_params);

enum _ecore_status_t
ecore_sp_eth_vport_start(struct ecore_hwfn *p_hwfn,
			 struct ecore_sp_vport_start_params *p_params);

/**
 * @brief - Starts an Rx queue, when queue_cid is already prepared
 *
 * @param p_hwfn
 * @param p_cid
 * @param bd_max_bytes
 * @param bd_chain_phys_addr
 * @param cqe_pbl_addr
 * @param cqe_pbl_size
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_eth_rxq_start_ramrod(struct ecore_hwfn *p_hwfn,
			   struct ecore_queue_cid *p_cid,
			   u16 bd_max_bytes,
			   dma_addr_t bd_chain_phys_addr,
			   dma_addr_t cqe_pbl_addr,
			   u16 cqe_pbl_size);

/**
 * @brief - Starts a Tx queue, where queue_cid is already prepared
 *
 * @param p_hwfn
 * @param p_cid
 * @param pbl_addr 
 * @param pbl_size
 * @param p_pq_params - parameters for choosing the PQ for this Tx queue
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_eth_txq_start_ramrod(struct ecore_hwfn *p_hwfn,
			   struct ecore_queue_cid *p_cid,
			   dma_addr_t pbl_addr, u16 pbl_size,
			   u16 pq_id);

u8 ecore_mcast_bin_from_mac(u8 *mac);

/**
 * @brief - ecore_configure_rfs_ntuple_filter
 *
 * This ramrod should be used to add or remove arfs hw filter
 *
 * @params p_hwfn
 * @params p_ptt
 * @params p_cb		Used for ECORE_SPQ_MODE_CB,where client would initialize
			it with cookie and callback function address, if not
			using this mode then client must pass NULL.
 * @params p_addr	p_addr is an actual packet header that needs to be
 *			filter. It has to mapped with IO to read prior to
 *			calling this, [contains 4 tuples- src ip, dest ip,
 *			src port, dest port].
 * @params length	length of p_addr header up to past the transport header.
 * @params qid		receive packet will be directed to this queue.
 * @params vport_id
 * @params b_is_add	flag to add or remove filter.
 *
 */
enum _ecore_status_t
ecore_configure_rfs_ntuple_filter(struct ecore_hwfn *p_hwfn,
				  struct ecore_ptt *p_ptt,
				  struct ecore_spq_comp_cb *p_cb,
				  dma_addr_t p_addr, u16 length,
				  u16 qid, u8 vport_id,
				  bool b_is_add);
#endif
