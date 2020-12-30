/*****************************************************************************\
 *  reservations.c - Slurm REST API reservations http operations handlers
 *****************************************************************************
 *  Copyright (C) 2020 UT-Battelle, LLC.
 *  Written by Matt Ezell <ezellma@ornl.gov>
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

#include "config.h"

#define _GNU_SOURCE

#include <search.h>
#include <stdint.h>
#include <unistd.h>

#include "slurm/slurm.h"

#include "src/common/ref.h"
#include "src/common/xassert.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

#include "src/slurmrestd/openapi.h"
#include "src/slurmrestd/operations.h"
#include "src/slurmrestd/xjson.h"

#include "src/slurmrestd/plugins/openapi/v0.0.37/api.h"

typedef enum {
	URL_TAG_UNKNOWN = 0,
	URL_TAG_RESERVATION,
	URL_TAG_RESERVATIONS,
} url_tag_t;

static int _dump_res(data_t *p, reserve_info_t *res)
{
	data_t *d = data_set_dict(data_list_append(p));

	data_t *flags = data_set_list(data_key_set(d, "flags"));
	data_set_string(data_key_set(d, "accounts"), res->accounts);
	data_set_string(data_key_set(d, "burst_buffer"), res->burst_buffer);
	data_set_int(data_key_set(d, "core_count"), res->core_cnt);
	data_set_int(data_key_set(d, "end_time"), res->end_time);
	data_set_string(data_key_set(d, "features"), res->features);
	if (res->flags & RESERVE_FLAG_MAINT)
		data_set_string(data_list_append(flags), "maint");
	if (res->flags & RESERVE_FLAG_DAILY)
		data_set_string(data_list_append(flags), "daily");
	if (res->flags & RESERVE_FLAG_WEEKLY)
		data_set_string(data_list_append(flags), "weekly");
	if (res->flags & RESERVE_FLAG_IGN_JOBS)
		data_set_string(data_list_append(flags), "ignore_jobs");
	if (res->flags & RESERVE_FLAG_ANY_NODES)
		data_set_string(data_list_append(flags), "any_nodes");
	if (res->flags & RESERVE_FLAG_STATIC)
		data_set_string(data_list_append(flags), "static");
	if (res->flags & RESERVE_FLAG_PART_NODES)
		data_set_string(data_list_append(flags), "part_nodes");
	if (res->flags & RESERVE_FLAG_OVERLAP)
		data_set_string(data_list_append(flags), "overlap");
	if (res->flags & RESERVE_FLAG_SPEC_NODES)
		data_set_string(data_list_append(flags), "spec_nodes");
	if (res->flags & RESERVE_FLAG_FIRST_CORES)
		data_set_string(data_list_append(flags), "first_cores");
	if (res->flags & RESERVE_FLAG_TIME_FLOAT)
		data_set_string(data_list_append(flags), "time_float");
	if (res->flags & RESERVE_FLAG_REPLACE)
		data_set_string(data_list_append(flags), "replace");
	if (res->flags & RESERVE_FLAG_ALL_NODES)
		data_set_string(data_list_append(flags), "all_nodes");
	if (res->flags & RESERVE_FLAG_PURGE_COMP)
		data_set_string(data_list_append(flags), "purge_comp");
	if (res->flags & RESERVE_FLAG_WEEKDAY)
		data_set_string(data_list_append(flags), "weekday");
	if (res->flags & RESERVE_FLAG_WEEKEND)
		data_set_string(data_list_append(flags), "weekend");
	if (res->flags & RESERVE_FLAG_FLEX)
		data_set_string(data_list_append(flags), "flex");
	if (res->flags & RESERVE_FLAG_NO_HOLD_JOBS)
		data_set_string(data_list_append(flags), "no_hold_jobs");
	if (res->flags & RESERVE_FLAG_REPLACE_DOWN)
		data_set_string(data_list_append(flags), "replace_down");
	if (res->flags & RESERVE_FLAG_MAGNETIC)
		data_set_string(data_list_append(flags), "magnetic");
	if (res->flags & RESERVE_FLAG_SKIP)
		data_set_string(data_list_append(flags), "skip");
	data_set_string(data_key_set(d, "licenses"), res->licenses);
	data_set_int(data_key_set(d, "max_start_delay"), res->max_start_delay);
	data_set_string(data_key_set(d, "name"), res->name);
	data_set_int(data_key_set(d, "node_count"), res->node_cnt);
	data_set_string(data_key_set(d, "node_list"), res->node_list);
	data_set_string(data_key_set(d, "partition"), res->partition);
	data_set_int(data_key_set(d, "purge_comp_time"), res->purge_comp_time);
	data_set_int(data_key_set(d, "start_time"), res->start_time);
	data_set_string(data_key_set(d, "tres"), res->tres_str);
	data_set_string(data_key_set(d, "users"), res->users);

	return SLURM_SUCCESS;
}

static int _op_handler_reservations(const char *context_id,
				  http_request_method_t method,
				  data_t *parameters, data_t *query,
				  int tag, data_t *d,
				  rest_auth_context_t *auth)
{
	int rc = SLURM_SUCCESS;
	data_t *errors = populate_response_format(d);
	data_t *reservations = data_set_list(data_key_set(d, "reservations"));
	char *name = NULL;
	reserve_info_msg_t *res_info_ptr = NULL;

	if (tag == URL_TAG_RESERVATION) {
		const data_t *res_name = data_key_get_const(parameters,
							    "reservation_name");
		if (!res_name || data_get_string_converted(res_name, &name) ||
		    !name)
			rc = ESLURM_RESERVATION_INVALID;
	}

	if (!rc)
		rc = slurm_load_reservations(0, &res_info_ptr);

	if (!rc && res_info_ptr) {
		int found = 0;

		for (int i = 0; !rc && (i < res_info_ptr->record_count); i++) {
			if ((tag == URL_TAG_RESERVATIONS) ||
			    !xstrcasecmp(
				    name,
				    res_info_ptr->reservation_array[i].name)) {
				rc = _dump_res(
					reservations,
					&res_info_ptr->reservation_array[i]);
				found++;
			}
		}

		if (!found)
			rc = ESLURM_RESERVATION_INVALID;
	}

	if (!res_info_ptr || res_info_ptr->record_count == 0)
		rc = ESLURM_RESERVATION_INVALID;

	if (rc) {
		data_t *e = data_set_dict(data_list_append(errors));
		data_set_string(data_key_set(e, "error"), slurm_strerror(rc));
		data_set_int(data_key_set(e, "errno"), rc);
	}

	slurm_free_reservation_info_msg(res_info_ptr);
	xfree(name);
	return rc;
}

extern void init_op_reservations(void)
{
	bind_operation_handler("/slurm/v0.0.37/reservations/",
			       _op_handler_reservations, URL_TAG_RESERVATIONS);
	bind_operation_handler("/slurm/v0.0.37/reservation/{reservation_name}",
			       _op_handler_reservations, URL_TAG_RESERVATION);
}

extern void destroy_op_reservations(void)
{
	unbind_operation_handler(_op_handler_reservations);
}