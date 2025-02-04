/** BEGIN COPYRIGHT BLOCK
 * Copyright (C) 2001 Sun Microsystems, Inc. Used by permission.
 * Copyright (C) 2005 Red Hat, Inc.
 * Copyright (C) 2010 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * License: GPL (version 3 or any later version).
 * See LICENSE for details. 
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


/* repl5_ruv.h - interface for replica update vector */

#ifndef REPL5_RUV
#define REPL5_RUV

#include "slapi-private.h"

#ifdef __cplusplus 
extern "C" {
#endif

typedef struct _ruv RUV;

enum 
{
	RUV_SUCCESS=0,
	RUV_BAD_DATA,
	RUV_NOTFOUND,
	RUV_MEMORY_ERROR,
	RUV_NSPR_ERROR,
	RUV_BAD_FORMAT,
	RUV_UNKNOWN_ERROR,
	RUV_ALREADY_EXIST,
    RUV_CSNPL_ERROR,
    RUV_COVERS_CSN
};

/* return values from ruv_compare_ruv */
enum 
{
	RUV_COMP_SUCCESS=0,
	RUV_COMP_NO_GENERATION, /* one or both of the RUVs is missing the replica generation */
	RUV_COMP_GENERATION_DIFFERS, /* the RUVs have different replica generations */
    /* one of the maxcsns in one of the RUVs is out of date with respect to the
       corresponding maxcsn in the corresponding replica in the other RUV */
	RUV_COMP_CSN_DIFFERS,
	RUV_COMP_RUV1_MISSING, /* ruv2 contains replicas not in ruv1 - CLEANRUV */
	RUV_COMP_RUV2_MISSING /* ruv1 contains replicas not in ruv2 */
};

/* used by ruv_replace_replica_purl_nolock */
#define RUV_LOCK 1
#define RUV_DONT_LOCK 0

#define RUV_COMP_IS_FATAL(ruvcomp) (ruvcomp && (ruvcomp < RUV_COMP_RUV1_MISSING))

typedef struct ruv_enum_data
{
    CSN *csn;
    CSN *min_csn;
} ruv_enum_data;

typedef int (*FNEnumRUV) (const ruv_enum_data *element, void *arg);
int ruv_init_new (const char *replGen, ReplicaId rid, const char *purl, RUV **ruv);
int ruv_init_from_bervals(struct berval** vals, RUV **ruv);
int ruv_init_from_slapi_attr(Slapi_Attr *attr, RUV **ruv);
int ruv_init_from_slapi_attr_and_check_purl(Slapi_Attr *attr, RUV **ruv, ReplicaId *rid);
RUV* ruv_dup (const RUV *ruv);
void ruv_destroy (RUV **ruv);
void ruv_copy_and_destroy (RUV **srcruv, RUV **destruv);
int ruv_replace_replica_purl (RUV *ruv, ReplicaId rid, const char *replica_purl);
int ruv_replace_replica_purl_nolock(RUV *ruv, ReplicaId rid, const char *replica_purl, int lock);
int ruv_delete_replica (RUV *ruv, ReplicaId rid); 
int ruv_add_replica (RUV *ruv, ReplicaId rid, const char *replica_purl);
int ruv_add_index_replica (RUV *ruv, ReplicaId rid, const char *replica_purl, int index);
PRBool ruv_contains_replica (const RUV *ruv, ReplicaId rid);
int ruv_get_largest_csn_for_replica(const RUV *ruv, ReplicaId rid, CSN **csn);
int ruv_get_smallest_csn_for_replica(const RUV *ruv, ReplicaId rid, CSN **csn);
int ruv_set_csns(RUV *ruv, const CSN *csn, const char *replica_purl);  
int ruv_set_csns_keep_smallest(RUV *ruv, const CSN *csn);  
int ruv_set_max_csn(RUV *ruv, const CSN *max_csn, const char *replica_purl);
int ruv_set_max_csn_ext(RUV *ruv, const CSN *max_csn, const char *replica_purl, PRBool must_be_greater);
int ruv_set_min_csn(RUV *ruv, const CSN *min_csn, const char *replica_purl);
const char *ruv_get_purl_for_replica(const RUV *ruv, ReplicaId rid);
char *ruv_get_replica_generation (const RUV *ruv);
void ruv_set_replica_generation (RUV *ruv, const char *generation);
PRBool ruv_covers_ruv(const RUV *covering_ruv, const RUV *covered_ruv);
PRBool ruv_covers_csn(const RUV *ruv, const CSN *csn);
PRBool ruv_covers_csn_strict(const RUV *ruv, const CSN *csn);
PRBool ruv_covers_csn_cleanallruv(const RUV *ruv, const CSN *csn);
int ruv_get_min_csn(const RUV *ruv, CSN **csn);
int ruv_get_min_csn_ext(const RUV *ruv, CSN **csn, int ignore_cleaned_rid);
int ruv_get_max_csn(const RUV *ruv, CSN **csn);
int ruv_get_max_csn_ext(const RUV *ruv, CSN **csn, int ignore_cleaned_rid);
int ruv_get_rid_max_csn(const RUV *ruv, CSN **csn, ReplicaId rid);
int ruv_get_rid_max_csn_ext(const RUV *ruv, CSN **csn, ReplicaId rid, int ignore_cleaned_rid);
int ruv_enumerate_elements (const RUV *ruv, FNEnumRUV fn, void *arg);
Slapi_Value **ruv_last_modified_to_valuearray(RUV *ruv);
Slapi_Value **ruv_to_valuearray(RUV *ruv);
int ruv_to_smod(const RUV *ruv, Slapi_Mod *smod);
int ruv_last_modified_to_smod(const RUV *ruv, Slapi_Mod *smod);
int ruv_to_bervals(const RUV *ruv, struct berval ***bvals);
PRInt32 ruv_replica_count (const RUV *ruv);
char **ruv_get_referrals(const RUV *ruv);
void ruv_dump(const RUV *ruv, char *ruv_name, PRFileDesc *prFile);
int ruv_add_csn_inprogress (RUV *ruv, const CSN *csn);
int ruv_cancel_csn_inprogress (RUV *ruv, const CSN *csn);
int ruv_update_ruv (RUV *ruv, const CSN *csn, const char *replica_purl, PRBool isLocal);
int ruv_move_local_supplier_to_first(RUV *ruv, ReplicaId rid);
int ruv_get_first_id_and_purl(RUV *ruv, ReplicaId *rid, char **replica_purl );
int ruv_local_contains_supplier(RUV *ruv, ReplicaId rid);
/* returns true if the ruv has any csns, false otherwise - used for testing
   whether or not an RUV is empty */
PRBool ruv_has_csns(const RUV *ruv);
PRBool ruv_has_both_csns(const RUV *ruv);
PRBool ruv_is_newer (Object *sruv, Object *cruv);
void ruv_force_csn_update_from_ruv(RUV *src_ruv, RUV *tgt_ruv, char *msg, int logLevel);
void ruv_force_csn_update (RUV *ruv, CSN *csn);
void ruv_insert_dummy_min_csn (RUV *ruv);
int ruv_compare_ruv(const RUV *ruv1, const char *ruv1name, const RUV *ruv2, const char *ruv2name, int strict, int loglevel);
#ifdef __cplusplus
}
#endif

#endif
