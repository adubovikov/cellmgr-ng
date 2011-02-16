/*
 * The SS7 Application part for forwarding or nat...
 *
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

#include <ss7_application.h>
#include <bsc_data.h>
#include <cellmgr_debug.h>
#include <msc_connection.h>
#include <sctp_m2ua.h>

#include <osmocore/talloc.h>

struct ss7_application *ss7_application_alloc(struct bsc_data *bsc)
{
	struct ss7_application *app;

	app = talloc_zero(bsc, struct ss7_application);
	if (!app) {
		LOGP(DINP, LOGL_ERROR, "Failed to create SS7 Application.\n");
		return NULL;
	}

	INIT_LLIST_HEAD(&app->sccp_connections);
	llist_add(&app->entry, &bsc->apps);
	app->nr = bsc->num_apps++;
	app->bsc = bsc;

	return app;
}

struct ss7_application *ss7_application_num(struct bsc_data *bsc, int num)
{
	struct ss7_application *ss7;

	llist_for_each_entry(ss7, &bsc->apps, entry)
		if (ss7->nr == num)
			return ss7;

	return NULL;
}

static int ss7_app_setup_stp(struct ss7_application *app,
			     int src_type, int src_num,
			     int dst_type, int dst_num)
{
	struct mtp_link_set *src, *dst;

	if (src_type != SS7_SET_LINKSET) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s source needs to be a linkset.\n",
		     app->nr, app->name);
		return -1;
	}

	if (dst_type != SS7_SET_LINKSET) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s destination needs to be a linkset.\n",
		     app->nr, app->name);
		return -1;
	}

	/* veryify the MTP Linkset */
	src = mtp_link_set_num(app->bsc, src_num);
	if (!src) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s source linkset not found with nr: %d.\n",
		     app->nr, app->name, src_num);
		return -2;
	}

	if (src->app) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s is using linkset %d/%s\n",
		      src->app->nr, src->app->name,
		      src->no, src->name);
		return -3;
	}

	/* veryify the MTP Linkset */
	dst = mtp_link_set_num(app->bsc, dst_num);
	if (!dst) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s destionation linkset not found with nr: %d.\n",
		     app->nr, app->name, dst_num);
		return -2;
	}

	if (dst->app) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s is using linkset %d/%s\n",
		      dst->app->nr, dst->app->name,
		      dst->no, dst->name);
		return -3;
	}

	/* now connect it */
	src->app = app;
	app->route_src.type = src_type;
	app->route_src.nr = src_num;
	app->route_src.set = src;
	app->route_src.msc = NULL;

	dst->app = app;
	app->route_dst.type = dst_type;
	app->route_dst.nr = dst_num;
	app->route_dst.set = dst;
	app->route_dst.msc = NULL;

	app->type = APP_STP;
	app->bsc->m2ua_trans->started = 1;

	/* TODO: start the applications here */
	return 0;
}

static int ss7_app_setup_relay(struct ss7_application *app, int type,
			       int src_type, int src_num, int dst_type, int dst_num)
{
	struct mtp_link_set *mtp;
	struct msc_connection *msc;

	/* verify the types */
	if (src_type != SS7_SET_LINKSET) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s source needs to be a linkset.\n",
		     app->nr, app->name);
		return -1;
	}

	if (dst_type != SS7_SET_MSC) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s dest needs to be a MSC.\n",
		     app->nr, app->name);
		return -1;
	}

	/* veryify the MTP Linkset */
	mtp = mtp_link_set_num(app->bsc, src_num);
	if (!mtp) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s source linkset not found with nr: %d.\n",
		     app->nr, app->name, src_num);
		return -2;
	}

	if (mtp->app) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s is using linkset %d/%s\n",
		      mtp->app->nr, mtp->app->name,
		      mtp->no, mtp->name);
		return -3;
	}

	/* verify the MSC connection */
	msc = msc_connection_num(app->bsc, dst_num);
	if (!msc) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s dest MSC not found with nr: %d.\n",
		     app->nr, app->name, dst_num);
		return -4;
	}

	if (msc->app) {
		LOGP(DINP, LOGL_ERROR,
		     "SS7 %d/%s is using MSC connection %d/%s\n",
		      msc->app->nr, msc->app->name,
		      msc->nr, msc->name);
		return -5;
	}


	/* now connect it and run the app */
	mtp->app = app;
	app->route_src.type = src_type;
	app->route_src.nr = src_num;
	app->route_src.set = mtp;
	app->route_src.msc = NULL;

	msc->app = app;
	app->route_dst.type = dst_type;
	app->route_dst.nr = dst_num;
	app->route_dst.set = NULL;
	app->route_dst.msc = msc;

	app->type = type;

	/* TODO: start the applications here */
	return 0;
}

int ss7_application_setup(struct ss7_application *ss7, int type,
			  int src_type, int src_num,
			  int dst_type, int dst_num)
{
	switch (type) {
	case APP_CELLMGR:
	case APP_RELAY:
		return ss7_app_setup_relay(ss7, type, src_type, src_num,
					   dst_type, dst_num);
		break;
	case APP_STP:
		return ss7_app_setup_stp(ss7, src_type, src_num,
					 dst_type, dst_num);
	default:
		LOGP(DINP, LOGL_ERROR,
		     "SS7 Application %d is not supported.\n", type);
		return -1;
	}
}
