/** BEGIN COPYRIGHT BLOCK
 * Copyright (C) 2013 Red Hat, Inc.
 * All rights reserved.
 *
 * License: GPL (version 3 or any later version).
 * See LICENSE for details. 
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/**
 * LDAP content synchronization plug-in
 */

#include <stdio.h>
#include <string.h>
#include "slapi-plugin.h"

#define PLUGIN_NAME              "content-sync-plugin"

#define SYNC_PLUGIN_SUBSYSTEM      "content-sync-plugin"
#define SYNC_PREOP_DESC		"content-sync-preop-subplugin"
#define SYNC_POSTOP_DESC		"content-sync-postop-subplugin"

#define OP_FLAG_SYNC_PERSIST 0x01

#define E_SYNC_REFRESH_REQUIRED 0x1000

#define CL_ATTR_CHANGENUMBER "changenumber"
#define CL_ATTR_ENTRYDN "targetDn"
#define CL_ATTR_UNIQUEID "targetUniqueId"
#define CL_ATTR_CHGTYPE "changetype"
#define CL_ATTR_NEWSUPERIOR "newsuperior"
#define CL_SRCH_BASE "cn=changelog"

#define SYNC_INVALID_CHANGENUM ((unsigned long)-1)

typedef struct sync_cookie {
	char *cookie_client_signature;
	char *cookie_server_signature;
	unsigned long cookie_change_info;
} Sync_Cookie;

typedef struct sync_update {
	char *upd_uuid;
	int	upd_chgtype;
	Slapi_Entry *upd_e;
} Sync_UpdateNode;

#define SYNC_CALLBACK_PREINIT (-1)

typedef struct sync_callback {
	Slapi_PBlock *orig_pb;
	unsigned long changenr;
	unsigned long change_start;
	int cb_err;
	Sync_UpdateNode *cb_updates;
} Sync_CallBackData;

int sync_register_operation_extension(void);
int sync_unregister_operation_entension(void);

int sync_srch_refresh_pre_search(Slapi_PBlock *pb);
int sync_srch_refresh_post_search(Slapi_PBlock *pb);
int sync_srch_refresh_pre_entry(Slapi_PBlock *pb);
int sync_srch_refresh_pre_result(Slapi_PBlock *pb);
int sync_del_persist_post_op(Slapi_PBlock *pb);
int sync_mod_persist_post_op(Slapi_PBlock *pb);
int sync_modrdn_persist_post_op(Slapi_PBlock *pb);
int sync_add_persist_post_op(Slapi_PBlock *pb);

int sync_parse_control_value( struct berval *psbvp, ber_int_t *mode, int *reload, char **cookie );
int sync_create_state_control( Slapi_Entry *e, LDAPControl **ctrlp, int type, Sync_Cookie *cookie);
int sync_create_sync_done_control( LDAPControl **ctrlp, int refresh, char *cookie);
int sync_intermediate_msg (Slapi_PBlock *pb, int tag, Sync_Cookie *cookie, char **uuids);
int sync_result_msg (Slapi_PBlock *pb, Sync_Cookie *cookie);
int sync_result_err (Slapi_PBlock *pb, int rc, char *msg);

Sync_Cookie *sync_cookie_create (Slapi_PBlock *pb);
void sync_cookie_update (Sync_Cookie *cookie, Slapi_Entry *ec);
Sync_Cookie *sync_cookie_parse (char *cookie);
int sync_cookie_isvalid (Sync_Cookie *testcookie, Sync_Cookie *refcookie);
void sync_cookie_free (Sync_Cookie **freecookie);
char * sync_cookie2str(Sync_Cookie *cookie);
int sync_number2int(char *nrstr);
unsigned long sync_number2ulong(char *nrstr);
char *sync_nsuniqueid2uuid(const char *nsuniqueid);

int sync_is_active (Slapi_Entry *e, Slapi_PBlock *pb);
int sync_is_active_scope (const Slapi_DN *dn, Slapi_PBlock *pb);

int sync_refresh_update_content(Slapi_PBlock *pb, Sync_Cookie *client_cookie, Sync_Cookie *session_cookie);
int sync_refresh_initial_content(Slapi_PBlock *pb, int persist, PRThread *tid, Sync_Cookie *session_cookie);
int sync_read_entry_from_changelog( Slapi_Entry *cl_entry, void *cb_data);
int sync_send_entry_from_changelog( Slapi_PBlock *pb, int chg_req, char *uniqueid);
void sync_send_deleted_entries (Slapi_PBlock *pb, Sync_UpdateNode *upd, int chg_count, Sync_Cookie *session_cookie);
void sync_send_modified_entries (Slapi_PBlock *pb, Sync_UpdateNode *upd, int chg_count);

int sync_persist_initialize (int argc, char **argv);
PRThread *sync_persist_add (Slapi_PBlock *pb);
int sync_persist_startup (PRThread *tid, Sync_Cookie *session_cookie);
int sync_persist_terminate_all ();
int sync_persist_terminate (PRThread *tid);

Slapi_PBlock *sync_pblock_copy(Slapi_PBlock *src);

/* prototype for functions not in slapi-plugin.h */
Slapi_ComponentId *plugin_get_default_component_id();


/*
 * Structures to handle the persitent phase of 
 * Content Synchronization Requests
 *
 * A queue of entries being to be sent by a particular persistent
 * sync thread
 *
 * will be created in post op plugins 
 */
typedef struct sync_queue_node {
	Slapi_Entry			*sync_entry;
	LDAPControl			*pe_ctrls[2]; /* XXX ?? XXX */
	struct sync_queue_node	*sync_next;
	int 			sync_chgtype;
} SyncQueueNode;

/*
 * Information about a single sync search
 *
 * will be created when a content sync control with 
 * mode == 3 is decoded
 */
typedef struct sync_request {
	Slapi_PBlock	*req_pblock;
	Slapi_Operation	*req_orig_op;
	PRLock		*req_lock;
	PRThread	*req_tid;
	char 		*req_orig_base;
	Slapi_Filter	*req_filter;
	PRInt32		req_complete;
	Sync_Cookie	*req_cookie;
	SyncQueueNode	*ps_eq_head;
	SyncQueueNode	*ps_eq_tail;
	int		req_active;
	struct sync_request	*req_next;
} SyncRequest;

/*
 * A list of established persistent synchronization searches.
 *
 * will be initialized at plugin initialization
 */
#define SYNC_MAX_CONCURRENT 10
typedef struct sync_request_list {
	Slapi_RWLock	*sync_req_rwlock;	/* R/W lock struct to serialize access */
	SyncRequest	*sync_req_head;	/* Head of list */
	PRLock	*sync_req_cvarlock;	/* Lock for cvar */
	PRCondVar	*sync_req_cvar;	/* ps threads sleep on this */
	int		sync_req_max_persist;
	int		sync_req_cur_persist;
} SyncRequestList;

#define SYNC_FLAG_ADD_STATE_CTRL	0x01
#define SYNC_FLAG_ADD_DONE_CTRL		0x02
#define SYNC_FLAG_NO_RESULT		0x04
#define SYNC_FLAG_SEND_INTERMEDIATE	0x08

typedef struct sync_op_info {
	int send_flag;		/* hint for preop plugins what to send */
	Sync_Cookie	*cookie;/* cookie to add in control */
	PRThread	*tid;	/* thread for persistent phase */
} SyncOpInfo;

