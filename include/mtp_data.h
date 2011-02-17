/*
 * (C) 2010-2011 by Holger Hans Peter Freyther <zecke@selfish.org>
 * (C) 2010-2011 by On-Waves
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef mtp_data_h
#define mtp_data_h

#include <osmocore/msgb.h>
#include <osmocore/timer.h>
#include <osmocore/utils.h>

struct bsc_data;
struct mtp_link;
struct mtp_level_3_mng *mng;
struct rate_ctr_group;
struct ss7_application;

/* MTP Level3 timers */

/* Timers for SS7 */
#define MTP_T1		12, 0
#define MTP_T2		30, 0
#define START_DELAY	 8, 0

enum ss7_link_type {
	SS7_LTYPE_NONE,
	SS7_LTYPE_UDP,
	SS7_LTYPE_M2UA,
};

/**
 * The state of the mtp_link in terms of layer3 and upwards
 */
struct mtp_link_set {
	struct llist_head entry;
	int nr;
	char *name;


	/* routing info.. */
	int dpc, opc, sccp_opc, isup_opc;
	int ni;
	int spare;


	/* internal state */
	/* the MTP1 link is up */
	int available;
	int running;
	int sccp_up;
	int linkset_up;

	int last_sls;

	struct llist_head links;
	int nr_links;
	struct mtp_link *slc[16];
	int sltm_once;

	/* ssn map */
	int supported_ssn[256];

	int pcap_fd;

	/* special handling */
	int pass_all_isup;

	/* statistics */
	struct rate_ctr_group *ctrg;

	/* custom data */
	struct ss7_application *app;
};

/**
 * One physical link to somewhere. This is the base
 * with the interface used by the mtp_link_set. There
 * will be specific implementations for M2UA, UDP and
 * other transport means.
 */
struct mtp_link {
	struct llist_head entry;

	int pcap_fd;
	struct mtp_link_set *set;

	int available;

	struct timer_list link_activate;

	/* link test routine */
	uint8_t test_ptrn[14];

	int blocked;

	int first_sls;
	int link_no;
	int sltm_pending;
	int was_up;

	int slta_misses;
	struct timer_list t1_timer;
	struct timer_list t2_timer;

	/* statistics */
	struct rate_ctr_group *ctrg;

	/* callback's to implement */
	int (*start)(struct mtp_link *);
	int (*write)(struct mtp_link *, struct msgb *msg);
	int (*shutdown)(struct mtp_link *);
	int (*reset)(struct mtp_link *data);
	int (*clear_queue)(struct mtp_link *data);

	/* private data */
	enum ss7_link_type type;
	void *data;
};


void mtp_link_set_stop(struct mtp_link_set *link);
void mtp_link_set_reset(struct mtp_link_set *link);
int mtp_link_set_data(struct mtp_link *link, struct msgb *msg);
int mtp_link_set_submit_sccp_data(struct mtp_link_set *link, int sls, const uint8_t *data, unsigned int length);
int mtp_link_set_submit_isup_data(struct mtp_link_set *link, int sls, const uint8_t *data, unsigned int length);

void mtp_link_set_init_slc(struct mtp_link_set *set);

void mtp_link_block(struct mtp_link *link);
void mtp_link_unblock(struct mtp_link *link);


/* to be implemented for MSU sending */
void mtp_link_submit(struct mtp_link *link, struct msgb *msg);
void mtp_link_set_forward_sccp(struct mtp_link_set *link, struct msgb *msg, int sls);
void mtp_link_set_forward_isup(struct mtp_link_set *link, struct msgb *msg, int sls);
void mtp_link_restart(struct mtp_link *link);
int mtp_link_set_send(struct mtp_link_set *set, struct msgb *msg);

/* link related routines */
void mtp_link_down(struct mtp_link *data);
void mtp_link_up(struct mtp_link *data);

void mtp_link_start_link_test(struct mtp_link *link);
void mtp_link_stop_link_test(struct mtp_link *link);
int mtp_link_slta(struct mtp_link *link, uint16_t l3_len, struct mtp_level_3_mng *mng);

void mtp_link_failure(struct mtp_link *fail);

/* internal routines */
struct msgb *mtp_msg_alloc(struct mtp_link_set *link);

/* link management */
struct mtp_link_set *mtp_link_set_alloc(struct bsc_data *bsc);
struct mtp_link_set *mtp_link_set_num(struct bsc_data *bsc, int num);

struct mtp_link *mtp_link_alloc(struct mtp_link_set *set);
struct mtp_link *mtp_link_num(struct mtp_link_set *set, int num);

#endif
