/*****************************************************************************\
 ** pmix_utils.h - Various PMIx utility functions
 *****************************************************************************
 *  Copyright (C) 2014-2015 Artem Polyakov. All rights reserved.
 *  Copyright (C) 2015-2017 Mellanox Technologies. All rights reserved.
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

#ifndef PMIXP_UTILS_H
#define PMIXP_UTILS_H

#include "pmixp_common.h"

extern int pmixp_count_digits_base10(uint32_t val);

void pmixp_free_buf(void *x);
int pmixp_usock_create_srv(char *path);
size_t pmixp_read_buf(int fd, void *buf, size_t count, int *shutdown,
		      bool blocking);
size_t pmixp_write_buf(int fd, void *buf, size_t count, int *shutdown,
		       bool blocking);
size_t pmixp_writev_buf(int sd, struct iovec *iov, size_t iovcnt,
			size_t offset, int *shutdown);

int pmixp_fd_set_nodelay(int fd);
bool pmixp_fd_read_ready(int fd, int *shutdown);
bool pmixp_fd_write_ready(int fd, int *shutdown);
int pmixp_srun_send(slurm_addr_t *addr, uint32_t len, char *data);
int pmixp_stepd_send(const char *nodelist, const char *address,
		     const char *data, uint32_t len, unsigned int start_delay,
		     unsigned int retry_cnt, int silent);
int pmixp_p2p_send(const char *nodename, const char *address, const char *data,
		   uint32_t len, unsigned int start_delay,
		   unsigned int retry_cnt, int silent);
int pmixp_rmdir_recursively(char *path);
int pmixp_fixrights(char *path, uid_t uid, mode_t mode);
int pmixp_mkdir(char *path, mode_t rights);

/* lightweight pmix list of pointers */
#define PMIXP_LIST_DEBUG 0
#define PMIXP_LIST_VAL(elem) (elem->data)

typedef struct pmixp_list_elem_s {
#ifndef NDEBUG
	void *lptr;
#endif
	void *data;
	struct pmixp_list_elem_s *next, *prev;
} pmixp_list_elem_t;

typedef struct pmixp_list_s {
	pmixp_list_elem_t *head, *tail;
	size_t count;
} pmixp_list_t;

/* PMIx list of pointers with element reuse */
typedef struct pmixp_rlist_s {
	pmixp_list_t list;
	pmixp_list_t *src_list;
	size_t pre_alloc;
} pmixp_rlist_t;

pmixp_list_elem_t *pmixp_list_elem_new(void);
void pmixp_list_elem_free(pmixp_list_elem_t *elem);
bool pmixp_list_empty(pmixp_list_t *l);
size_t pmixp_list_count(pmixp_list_t *l);
void pmixp_list_init_pre(pmixp_list_t *l, pmixp_list_elem_t *h,
        pmixp_list_elem_t *t);
pmixp_list_fini_pre(pmixp_list_t *l, pmixp_list_elem_t **h,
        pmixp_list_elem_t **t);
void pmixp_list_init(pmixp_list_t *l);
void pmixp_list_fini(pmixp_list_t *l);
void pmixp_list_enq(pmixp_list_t *l, pmixp_list_elem_t *elem);
pmixp_list_elem_t *pmixp_list_deq(pmixp_list_t *l);
void pmixp_list_push(pmixp_list_t *l, pmixp_list_elem_t *elem);
pmixp_list_elem_t *pmixp_list_pop(pmixp_list_t *l);
pmixp_list_elem_t *pmixp_list_rem(pmixp_list_t *l, pmixp_list_elem_t *elem);
pmixp_list_elem_t *pmixp_list_begin(pmixp_list_t *l);
pmixp_list_elem_t *pmixp_list_end(pmixp_list_t *l);
pmixp_list_elem_t *pmixp_list_next(pmixp_list_t *l, pmixp_list_elem_t *cur);
pmixp_list_elem_t *__pmixp_rlist_get_free(pmixp_list_t *l, size_t pre_alloc);
void pmixp_rlist_init(pmixp_rlist_t *l, pmixp_list_t *elem_src,
        size_t pre_alloc);
void pmixp_rlist_fini(pmixp_rlist_t *l);
bool pmixp_rlist_empty(pmixp_rlist_t *l);
size_t pmixp_rlist_count(pmixp_rlist_t *l);
void pmixp_rlist_enq(pmixp_rlist_t *l, void *ptr);
void *pmixp_rlist_deq(pmixp_rlist_t *l);
void pmixp_rlist_push(pmixp_rlist_t *l, void *ptr);
void *pmixp_rlist_pop(pmixp_rlist_t *l);
pmixp_list_elem_t *pmixp_rlist_begin(pmixp_rlist_t *l);
pmixp_list_elem_t *pmixp_rlist_end(pmixp_rlist_t *l);
pmixp_list_elem_t *pmixp_rlist_next(pmixp_rlist_t *l, pmixp_list_elem_t *cur);
pmixp_list_elem_t *pmixp_rlist_rem(pmixp_rlist_t *l, pmixp_list_elem_t *elem);

#endif /* PMIXP_UTILS_H*/
