/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AppArmor security module
 *
 * This file contains AppArmor auditing function definitions.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 */

#ifndef __AA_AUDIT_H
#define __AA_AUDIT_H

#include <linux/audit.h>
#include <linux/fs.h>
#include <linux/lsm_audit.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "file.h"
#include "label.h"
#include "notify.h"

extern const char *const audit_mode_names[];
#define AUDIT_MAX_INDEX 5
enum audit_mode {
	AUDIT_NORMAL,		/* follow normal auditing of accesses */
	AUDIT_QUIET_DENIED,	/* quiet all denied access messages */
	AUDIT_QUIET,		/* quiet all messages */
	AUDIT_NOQUIET,		/* do not quiet audit messages */
	AUDIT_ALL		/* audit all accesses */
};

enum audit_type {
	AUDIT_APPARMOR_AUDIT,
	AUDIT_APPARMOR_ALLOWED,
	AUDIT_APPARMOR_DENIED,
	AUDIT_APPARMOR_HINT,
	AUDIT_APPARMOR_STATUS,
	AUDIT_APPARMOR_ERROR,
	AUDIT_APPARMOR_KILL,
	AUDIT_APPARMOR_USER,
	AUDIT_APPARMOR_AUTO
};

#define OP_NULL NULL

#define OP_SYSCTL "sysctl"
#define OP_CAPABLE "capable"

#define OP_UNLINK "unlink"
#define OP_MKDIR "mkdir"
#define OP_RMDIR "rmdir"
#define OP_MKNOD "mknod"
#define OP_TRUNC "truncate"
#define OP_LINK "link"
#define OP_SYMLINK "symlink"
#define OP_RENAME_SRC "rename_src"
#define OP_RENAME_DEST "rename_dest"
#define OP_CHMOD "chmod"
#define OP_CHOWN "chown"
#define OP_GETATTR "getattr"
#define OP_OPEN "open"

#define OP_FRECEIVE "file_receive"
#define OP_FPERM "file_perm"
#define OP_FLOCK "file_lock"
#define OP_FMMAP "file_mmap"
#define OP_FMPROT "file_mprotect"
#define OP_INHERIT "file_inherit"

#define OP_PIVOTROOT "pivotroot"
#define OP_MOUNT "mount"
#define OP_UMOUNT "umount"

#define OP_CREATE "create"
#define OP_POST_CREATE "post_create"
#define OP_BIND "bind"
#define OP_CONNECT "connect"
#define OP_LISTEN "listen"
#define OP_ACCEPT "accept"
#define OP_SENDMSG "sendmsg"
#define OP_RECVMSG "recvmsg"
#define OP_GETSOCKNAME "getsockname"
#define OP_GETPEERNAME "getpeername"
#define OP_GETSOCKOPT "getsockopt"
#define OP_SETSOCKOPT "setsockopt"
#define OP_SHUTDOWN "socket_shutdown"

#define OP_PTRACE "ptrace"
#define OP_SIGNAL "signal"

#define OP_EXEC "exec"

#define OP_CHANGE_HAT "change_hat"
#define OP_CHANGE_PROFILE "change_profile"
#define OP_CHANGE_ONEXEC "change_onexec"
#define OP_STACK "stack"
#define OP_STACK_ONEXEC "stack_onexec"

#define OP_SETPROCATTR "setprocattr"
#define OP_SETRLIMIT "setrlimit"

#define OP_PROF_REPL "profile_replace"
#define OP_PROF_LOAD "profile_load"
#define OP_PROF_RM "profile_remove"

#define OP_USERNS_CREATE "userns_create"

#define OP_URING_OVERRIDE "uring_override"
#define OP_URING_SQPOLL "uring_sqpoll"

struct apparmor_audit_data {
	int error;
	int type;
	u16 class;
	const char *op;
	const struct cred *subj_cred;
	struct aa_label *subj_label;
	const char *name;
	const char *info;
	u32 request;
	u32 denied;
	u8 flags;		/* temporary - move to audit_node or knotif */
	struct task_struct *subjtsk;

	union {
		/* these entries require a custom callback fn */
		struct {
			struct aa_label *peer;
			union {
				struct {
					const char *target;
					kuid_t ouid;
				} fs;
				struct {
					int rlim;
					unsigned long max;
				} rlim;
				struct {
					int signal;
					int unmappedsig;
				};
				struct {
					int type, protocol;
					struct sock *peer_sk;
					void *addr;
					int addrlen;
				} net;
				struct {
					kuid_t fsuid;
					kuid_t ouid;
				} mq;
			};
		};
		struct {
			struct aa_profile *profile;
			const char *ns;
			long pos;
		} iface;
		struct {
			const char *src_name;
			const char *type;
			const char *trans;
			const char *data;
			unsigned long flags;
		} mnt;
		struct {
			struct aa_label *target;
		} uring;
	};

	struct common_audit_data common;
};

struct aa_audit_node {
	struct kref count;
	struct apparmor_audit_data data;
	struct list_head list;
	struct aa_knotif knotif;
};
extern struct kmem_cache *aa_audit_slab;

static inline struct aa_audit_node *aa_alloc_audit_node(gfp_t gfp)
{
	return kmem_cache_zalloc(aa_audit_slab, gfp);
}


struct aa_audit_cache {
	spinlock_t lock;
	int size;
	struct list_head head;
};

static inline void aa_audit_cache_init(struct aa_audit_cache *cache)
{
	cache->size = 0;
	spin_lock_init(&cache->lock);
	INIT_LIST_HEAD(&cache->head);
}

struct aa_audit_node *aa_audit_cache_find(struct aa_audit_cache *cache,
					  struct apparmor_audit_data *ad);
struct aa_audit_node *aa_audit_cache_insert(struct aa_audit_cache *cache,
					    struct aa_audit_node *node);
void aa_audit_cache_update_ent(struct aa_audit_cache *cache,
			       struct aa_audit_node *node,
			       struct apparmor_audit_data *data);
void aa_audit_cache_destroy(struct aa_audit_cache *cache);



/* macros for dealing with  apparmor_audit_data structure */
#define aad(SA) (container_of(SA, struct apparmor_audit_data, common))
#define aad_of_va(VA) aad((struct common_audit_data *)(VA))

#define DEFINE_AUDIT_DATA(NAME, T, C, X)				\
	/* TODO: cleanup audit init so we don't need _aad = {0,} */	\
	struct apparmor_audit_data NAME = {				\
		.class = (C),						\
		.op = (X),                                              \
		.subjtsk = NULL,                                        \
		.common.type = (T),					\
		.common.u.tsk = NULL,					\
		.common.apparmor_audit_data = &NAME,			\
	};

void aa_audit_msg(int type, struct apparmor_audit_data *ad,
		  void (*cb) (struct audit_buffer *, void *));
int aa_audit(int type, struct aa_profile *profile,
	     struct apparmor_audit_data *ad,
	     void (*cb) (struct audit_buffer *, void *));

#define aa_audit_error(ERROR, AD, CB)				\
({								\
	(AD)->error = (ERROR);					\
	aa_audit_msg(AUDIT_APPARMOR_ERROR, (AD), (CB));		\
	(AD)->error;					\
})


static inline int complain_error(int error)
{
	if (error == -EPERM || error == -EACCES)
		return 0;
	return error;
}

void aa_audit_rule_free(void *vrule);
int aa_audit_rule_init(u32 field, u32 op, char *rulestr, void **vrule, gfp_t gfp);
int aa_audit_rule_known(struct audit_krule *rule);
int aa_audit_rule_match(struct lsmblob *blob, u32 field, u32 op, void *vrule);


void aa_audit_node_free_kref(struct kref *kref);
struct aa_audit_node *aa_dup_audit_data(struct apparmor_audit_data *orig,
					gfp_t gfp);
long aa_audit_data_cmp(struct apparmor_audit_data *lhs,
		       struct apparmor_audit_data *rhs);


static inline struct aa_audit_node *aa_get_audit_node(struct aa_audit_node *node)
{
	if (node)
		kref_get(&(node->count));

	return node;
}

static inline void aa_put_audit_node(struct aa_audit_node *node)
{
	if (node)
		kref_put(&node->count, aa_audit_node_free_kref);
}


#endif /* __AA_AUDIT_H */
