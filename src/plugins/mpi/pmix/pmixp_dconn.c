/*****************************************************************************\
 **  pmix_dconn.c - direct connection module
 *****************************************************************************
 *  Copyright (C) 2017      Mellanox Technologies. All rights reserved.
 *  Written by Artem Polyakov <artpol84@gmail.com, artemp@mellanox.com>.
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 \*****************************************************************************/

#include "pmixp_dconn.h"
#include "pmixp_dconn_tcp.h"
#include "pmixp_dconn_ucx.h"

pmixp_dconn_t *_pmixp_dconn_conns = NULL;
uint32_t _pmixp_dconn_conn_cnt = 0;
pmixp_dconn_handlers_t _pmixp_dconn_h;

static pmixp_dconn_progress_type_t _progress_type;
static pmixp_dconn_conn_type_t _conn_type;
static int _poll_fd = -1;
static char *ep_data = NULL;
static size_t ep_len = 0;

int pmixp_dconn_init(int node_cnt, pmixp_p2p_data_t direct_hdr)
{
	int i;
	memset(&_pmixp_dconn_h, 0, sizeof(_pmixp_dconn_h));

#ifdef HAVE_UCX
	if (pmixp_info_srv_direct_conn_ucx()) {
		_poll_fd = pmixp_dconn_ucx_prepare(&_pmixp_dconn_h,
						   &ep_data, &ep_len);
		_progress_type = PMIXP_DCONN_PROGRESS_HW;
		_conn_type = PMIXP_DCONN_CONN_TYPE_ONESIDE;
	} else
#endif
	{
		_poll_fd = pmixp_dconn_tcp_prepare(&_pmixp_dconn_h,
						   &ep_data, &ep_len);
		_progress_type = PMIXP_DCONN_PROGRESS_SW;
		_conn_type = PMIXP_DCONN_CONN_TYPE_TWOSIDE;
	}

	if (SLURM_ERROR == _poll_fd) {
		PMIXP_ERROR("Cannot get polling fd");
		return SLURM_ERROR;
	}
	_pmixp_dconn_conns = xmalloc(sizeof(*_pmixp_dconn_conns) * node_cnt);
	_pmixp_dconn_conn_cnt = node_cnt;
	for (i=0; i<_pmixp_dconn_conn_cnt; i++) {
		slurm_mutex_init(&_pmixp_dconn_conns[i].lock);
		_pmixp_dconn_conns[i].nodeid = i;
		_pmixp_dconn_conns[i].state = PMIXP_DIRECT_INIT;
		_pmixp_dconn_conns[i].priv = _pmixp_dconn_h.init(i, direct_hdr);
	}
	return SLURM_SUCCESS;
}

void pmixp_dconn_fini()
{
	int i;
#ifdef HAVE_UCX
	if (pmixp_info_srv_direct_conn_ucx()) {
		pmixp_dconn_ucx_stop();
	}
#endif
	for (i=0; i<_pmixp_dconn_conn_cnt; i++) {
		slurm_mutex_destroy(&_pmixp_dconn_conns[i].lock);
		_pmixp_dconn_h.fini(_pmixp_dconn_conns[i].priv);
	}

#ifdef HAVE_UCX
	if (pmixp_info_srv_direct_conn_ucx()) {
		pmixp_dconn_ucx_finalize();
	} else
#endif
	{
		pmixp_dconn_tcp_finalize();
	}

	xfree(_pmixp_dconn_conns);
	_pmixp_dconn_conn_cnt = 0;
}

int pmixp_dconn_connect_do(pmixp_dconn_t *dconn, void *ep_data,
			   size_t ep_len, void *init_msg)
{
	return _pmixp_dconn_h.connect(dconn->priv, ep_data, ep_len, init_msg);
}

pmixp_dconn_progress_type_t pmixp_dconn_progress_type()
{
	return _progress_type;
}

pmixp_dconn_conn_type_t pmixp_dconn_connect_type()
{
	return _conn_type;
}

int pmixp_dconn_poll_fd()
{
	return _poll_fd;
}

size_t pmixp_dconn_ep_len()
{
	return ep_len;
}

char *pmixp_dconn_ep_data()
{
	return ep_data;
}

pmixp_dconn_t *pmixp_dconn_lock(int nodeid)
{
	xassert(nodeid < _pmixp_dconn_conn_cnt);
	slurm_mutex_lock(&_pmixp_dconn_conns[nodeid].lock);
	return &_pmixp_dconn_conns[nodeid];
}

void pmixp_dconn_unlock(pmixp_dconn_t *dconn)
{
	pmixp_dconn_verify(dconn);
	slurm_mutex_unlock(&dconn->lock);
}

pmixp_dconn_state_t pmixp_dconn_state(pmixp_dconn_t *dconn)
{
	pmixp_dconn_verify(dconn);
	return dconn->state;
}

void pmixp_dconn_req_sent(pmixp_dconn_t *dconn)
{
	if (PMIXP_DIRECT_INIT != dconn->state) {
		PMIXP_ERROR("State machine violation, when transition "
			    "to PORT_SENT from %d",
			    (int)dconn->state);
		xassert(PMIXP_DIRECT_INIT == dconn->state);
		abort();
	}
	dconn->state = PMIXP_DIRECT_EP_SENT;
}

int pmixp_dconn_send(pmixp_dconn_t *dconn, void *msg)
{
	return _pmixp_dconn_h.send(dconn->priv, msg);
}

void pmixp_dconn_regio(eio_handle_t *h)
{
	return _pmixp_dconn_h.regio(h);
}

bool pmixp_dconn_require_connect(pmixp_dconn_t *dconn, bool *send_init)
{
	*send_init = false;
	switch( pmixp_dconn_state(dconn) ){
	case PMIXP_DIRECT_INIT:
		*send_init = true;
		return true;
	case PMIXP_DIRECT_EP_SENT:{
		switch (pmixp_dconn_connect_type()) {
		case PMIXP_DCONN_CONN_TYPE_TWOSIDE: {
			if( dconn->nodeid < pmixp_info_nodeid()){
				*send_init = true;
				return true;
			} else {
				/* just ignore this connection,
				 * remote side will come with counter-
				 * connection
				 */
				return false;
			}
		}
		case PMIXP_DCONN_CONN_TYPE_ONESIDE:
			*send_init = false;
			return true;
		default:
			/* shouldn't happen */
			PMIXP_ERROR("Unexpected direct connection "
				    "semantics type: %d",
				    pmixp_dconn_connect_type());
			xassert(0 && pmixp_dconn_connect_type());
			abort();
		}
		break;
	}
	case PMIXP_DIRECT_CONNECTED:
		PMIXP_DEBUG("Trying to re-establish the connection");
		return false;
	default:
		/* shouldn't happen */
		PMIXP_ERROR("Unexpected direct connection state: "
			    "PMIXP_DIRECT_NONE");
		xassert(0 && pmixp_dconn_state(dconn));
		abort();
	}
	return false;
}

int pmixp_dconn_connect(pmixp_dconn_t *dconn, void *ep_data, int ep_len,
        void *init_msg)
{
	int rc;
	/* establish the connection */
	rc = pmixp_dconn_connect_do(dconn, ep_data, ep_len, init_msg);
	if (SLURM_SUCCESS == rc){
		dconn->state = PMIXP_DIRECT_CONNECTED;
	} else {
		/*
		 * Abort the application - we can't do what user requested.
		 * Make sure to provide enough info
		 */
		char *nodename = pmixp_info_job_host(dconn->nodeid);
		xassert(nodename);
		if (NULL == nodename) {
			PMIXP_ERROR("Bad nodeid = %d in the incoming message",
				    dconn->nodeid);
			abort();
		}
		PMIXP_ERROR("Cannot establish direct connection to %s (%d)",
			    nodename, dconn->nodeid);
		xfree(nodename);
		pmixp_debug_hang(0); /* enable hang to debug this! */
		slurm_kill_job_step(pmixp_info_jobid(),
				    pmixp_info_stepid(), SIGKILL);
	}
	return rc;
}

pmixp_io_engine_t *pmixp_dconn_engine(pmixp_dconn_t *dconn)
{
	pmixp_dconn_verify(dconn);
	xassert( PMIXP_DCONN_PROGRESS_SW == pmixp_dconn_progress_type(dconn));
	if( PMIXP_DCONN_PROGRESS_SW == pmixp_dconn_progress_type(dconn) ){
		return _pmixp_dconn_h.getio(dconn->priv);
	}
	return NULL;
}

pmixp_dconn_t *pmixp_dconn_accept(int nodeid, int fd)
{
	if( PMIXP_DCONN_PROGRESS_SW != pmixp_dconn_progress_type() ){
		PMIXP_ERROR("Accept is not supported by direct connection "
			    "of type %d",
			    (int)pmixp_dconn_progress_type());
		xassert(PMIXP_DCONN_PROGRESS_SW ==
			pmixp_dconn_progress_type());
		return NULL;
	}
	pmixp_dconn_t *dconn = pmixp_dconn_lock(nodeid);
	xassert(dconn);
	pmixp_io_engine_t *eng = _pmixp_dconn_h.getio(dconn->priv);
	xassert( NULL != eng );

	if( PMIXP_DIRECT_EP_SENT == pmixp_dconn_state(dconn) ){
		/* we request this connection some time ago
		 * and now we finishing it's establishment
		 */
		pmixp_fd_set_nodelay(fd);
		pmixp_io_attach(eng, fd);
		dconn->state = PMIXP_DIRECT_CONNECTED;
	} else {
		/* shouldn't happen */
		PMIXP_ERROR("Unexpected direct connection state: %d",
			    (int)pmixp_dconn_state(dconn));
		xassert(PMIXP_DIRECT_EP_SENT == pmixp_dconn_state(dconn));
		pmixp_dconn_unlock(dconn);
		return NULL;
	}
	return dconn;
}

void pmixp_dconn_disconnect(pmixp_dconn_t *dconn)
{
	switch( pmixp_dconn_state(dconn) ){
	case PMIXP_DIRECT_INIT:
	case PMIXP_DIRECT_EP_SENT:
		break;
	case PMIXP_DIRECT_CONNECTED:{
		pmixp_io_engine_t *eng = _pmixp_dconn_h.getio(dconn->priv);
		int fd = pmixp_io_detach(eng);
		close(fd);
		break;
	}
	default:
		/* shouldn't happen */
		PMIXP_ERROR("Unexpected direct connection state: "
			    "PMIXP_DIRECT_NONE");
		xassert(0 && pmixp_dconn_state(dconn));
		abort();
	}

	dconn->state = PMIXP_DIRECT_INIT;
}
