/** BEGIN COPYRIGHT BLOCK
 * Copyright (C) 2001 Sun Microsystems, Inc. Used by permission.
 * Copyright (C) 2005 Red Hat, Inc.
 * All rights reserved.
 *
 * License: GPL (version 3 or any later version).
 * See LICENSE for details. 
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


/* Handles computed attributes for entries as they're returned to the client */

#include "slap.h"
#include "proto-slap.h"


/* Structure used to pass the context needed for completing a computed attribute operation */
struct _computed_attr_context {
	BerElement *ber;
	int attrsonly;
	char *requested_type;
	Slapi_PBlock *pb;
};


struct _compute_evaluator {
	struct _compute_evaluator *next;
	slapi_compute_callback_t function;
	int rootonly;
};
typedef struct _compute_evaluator compute_evaluator;

static PRBool startup_completed = PR_FALSE;

static compute_evaluator *compute_evaluators = NULL;
static Slapi_RWLock *compute_evaluators_lock = NULL;
static PRBool require_compute_evaluator_lock = PR_FALSE;

static int
compute_stock_evaluator(computed_attr_context *c,char* type,Slapi_Entry *e,slapi_compute_output_t outputfn);

struct _compute_rewriter {
	struct _compute_rewriter *next;
	slapi_search_rewrite_callback_t function;
};
typedef struct _compute_rewriter compute_rewriter;

static compute_rewriter *compute_rewriters = NULL;
static Slapi_RWLock *compute_rewriters_lock = NULL;
static PRBool require_compute_rewriters_lock = PR_FALSE;

/* Function called by evaluators to have the value output */
static int
compute_output_callback(computed_attr_context *c,Slapi_Attr *a , Slapi_Entry *e)
{
	return encode_attr (c->pb, c->ber, e, a, c->attrsonly, c->requested_type);
}

static
int compute_call_evaluators_nolock(computed_attr_context *c,slapi_compute_output_t outfn,char *type,Slapi_Entry *e)
{
        int rc = -1;
        compute_evaluator *current = NULL;
        
        for (current = compute_evaluators; (current != NULL) && (-1 == rc); current = current->next) {
		if (current->rootonly) {
			int isroot;
			slapi_pblock_get(c->pb, SLAPI_REQUESTOR_ISROOT, &isroot);
			if (!isroot) {
				continue;
			}
		}
                rc = (*(current->function))(c,type,e,outfn);
        }
        return rc;
}

static int
compute_call_evaluators(computed_attr_context *c,slapi_compute_output_t outfn,char *type,Slapi_Entry *e)
{
	int rc = -1;
        int need_lock = require_compute_evaluator_lock;
        
	/* Walk along the list (locked) calling the evaluator functions util one says yes, an error happens, or we finish */

        if (need_lock) {
                slapi_rwlock_rdlock(compute_evaluators_lock);
        }
       
        rc = compute_call_evaluators_nolock(c, outfn, type, e);
        
        if (need_lock) {
                slapi_rwlock_unlock(compute_evaluators_lock);
        }   

	return rc;
}

/* Returns : -1 if no attribute matched the requested type */
/*			 0  if one matched and it was processed without error */
/*           >0 if an error happened */
int
compute_attribute(char *type, Slapi_PBlock *pb,BerElement *ber,Slapi_Entry *e,int attrsonly,char *requested_type)
{
	computed_attr_context context;

	context.ber = ber;
	context.attrsonly = attrsonly;
	context.requested_type = requested_type;
	context.pb = pb;

	return compute_call_evaluators(&context,compute_output_callback,type,e);
}

static int
compute_stock_evaluator(computed_attr_context *c,char* type,Slapi_Entry *e,slapi_compute_output_t outputfn)
{
	int rc= -1;
	static char* subschemasubentry = "subschemasubentry";

	if ( strcasecmp (type, subschemasubentry ) == 0)
	{
		Slapi_Attr our_attr;
		slapi_attr_init(&our_attr, subschemasubentry);
		our_attr.a_flags = SLAPI_ATTR_FLAG_OPATTR;
		valueset_add_string(&our_attr, &our_attr.a_present_values,SLAPD_SCHEMA_DN,CSN_TYPE_UNKNOWN,NULL);
		rc = (*outputfn) (c, &our_attr, e);
		attr_done(&our_attr);
		return (rc);
	}
	return rc; /* I see no ships */
}

static void
compute_add_evaluator_nolock(slapi_compute_callback_t function, compute_evaluator *new_eval, int rootonly)
{
    new_eval->next = compute_evaluators;
    new_eval->function = function;
    new_eval->rootonly = rootonly;
    compute_evaluators = new_eval;
}
int slapi_compute_add_evaluator(slapi_compute_callback_t function)
{
	return slapi_compute_add_evaluator_ext(function, 0);
}
int slapi_compute_add_evaluator_ext(slapi_compute_callback_t function, int rootonly)
{
	int rc = 0;
	compute_evaluator *new_eval = NULL;
	PR_ASSERT(NULL != function);
	PR_ASSERT(NULL != compute_evaluators_lock);
        if (startup_completed) {
            /* We are now in multi-threaded and we still add
             * a attribute evaluator.
             * switch to use locking mechanimsm
             */
            require_compute_evaluator_lock = PR_TRUE;
        }

	new_eval = (compute_evaluator *)slapi_ch_calloc(1,sizeof (compute_evaluator));
	if (NULL == new_eval) {
		rc = ENOMEM;
	} else {
                int need_lock = require_compute_evaluator_lock;
                
                if (need_lock) {
                    slapi_rwlock_wrlock(compute_evaluators_lock);
                }
                
                compute_add_evaluator_nolock(function, new_eval, rootonly);
                
                if (need_lock) {
                    slapi_rwlock_unlock(compute_evaluators_lock);
                }
	}

	return rc;
}

/* Called when */
void
compute_plugins_started()
{
    startup_completed = PR_TRUE;
}

/* Call this on server startup, before the first LDAP operation is serviced */
int compute_init()
{
	/* Initialize the lock */
	compute_evaluators_lock = slapi_new_rwlock();
	if (NULL == compute_evaluators_lock) {
		/* Out of resources */
		return ENOMEM;
	}
	compute_rewriters_lock = slapi_new_rwlock();
	if (NULL == compute_rewriters_lock) {
		/* Out of resources */
		return ENOMEM;
	}
	/* Now add the stock evaluators to the list */
	return slapi_compute_add_evaluator(compute_stock_evaluator);	
}

/* Call this on server shutdown, after the last LDAP operation has
terminated */
int compute_terminate()
{
	/* Free the list */
	if (NULL != compute_evaluators_lock) {
		compute_evaluator *current = compute_evaluators;
		slapi_rwlock_wrlock(compute_evaluators_lock);
		while (current != NULL) {
			compute_evaluator *asabird = current;
			current = current->next;
			slapi_ch_free((void **)&asabird);
		}
		slapi_rwlock_unlock(compute_evaluators_lock);
		/* Free the lock */
		slapi_destroy_rwlock(compute_evaluators_lock);
	}
	if (NULL != compute_rewriters_lock) {
		compute_rewriter *current = compute_rewriters;
		slapi_rwlock_wrlock(compute_rewriters_lock);
		while (current != NULL) {
			compute_rewriter *asabird = current;
			current = current->next;
			slapi_ch_free((void **)&asabird);
		}
		slapi_rwlock_unlock(compute_rewriters_lock);
		slapi_destroy_rwlock(compute_rewriters_lock);
	}	
	return 0;
}

static void
compute_add_search_rewrite_nolock(slapi_search_rewrite_callback_t function, compute_rewriter *new_rewriter)
{
    new_rewriter->next = compute_rewriters;
    new_rewriter->function = function;
    compute_rewriters = new_rewriter;
}

/* Functions dealing with re-writing of search filters */

int slapi_compute_add_search_rewriter(slapi_search_rewrite_callback_t function)
{
	int rc = 0;
	compute_rewriter *new_rewriter = NULL;
	PR_ASSERT(NULL != function);
	PR_ASSERT(NULL != compute_rewriters_lock);
        if (startup_completed) {
            /* We are now in multi-threaded and we still add
             * a filter rewriter.
             * switch to use locking mechanimsm
             */
            require_compute_rewriters_lock = PR_TRUE;
        }
	new_rewriter = (compute_rewriter *)slapi_ch_calloc(1,sizeof (compute_rewriter));
	if (NULL == new_rewriter) {
		rc = ENOMEM;
	} else {
                int need_lock = require_compute_rewriters_lock;
                
		if (need_lock) {
                    slapi_rwlock_wrlock(compute_rewriters_lock);
                }
                
                compute_add_search_rewrite_nolock(function, new_rewriter);
                
                if (need_lock) {
                    slapi_rwlock_unlock(compute_rewriters_lock);
                }
	}
	return rc;
}

static
int compute_rewrite_search_filter_nolock(Slapi_PBlock *pb)
{
        int rc = -1;
        compute_rewriter *current = NULL;
        
        for (current = compute_rewriters; (current != NULL) && (-1 == rc); current = current->next) {
                rc = (*(current->function))(pb);
                /* Meaning of the return code :
                   -1 : keep looking
                    0 : rewrote OK
                    1 : refuse to do this search
                    2 : operations error
                */
        }
        return(rc);
}

int compute_rewrite_search_filter(Slapi_PBlock *pb)
{
	/* Iterate through the listed rewriters until one says it matched */
	int rc = -1;
        int need_lock = require_compute_rewriters_lock;

	/* Walk along the list (locked) calling the evaluator functions util one says yes, an error happens, or we finish */
        if (need_lock) {
                slapi_rwlock_rdlock(compute_rewriters_lock);
        }
        
        rc = compute_rewrite_search_filter_nolock(pb);
        
        if (need_lock) {
                slapi_rwlock_unlock(compute_rewriters_lock);
        }
	return rc;

}


