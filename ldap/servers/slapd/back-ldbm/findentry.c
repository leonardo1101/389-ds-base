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

/* findentry.c - find a database entry, obeying referrals (& aliases?) */

#include "back-ldbm.h"


static struct backentry *find_entry_internal_dn(Slapi_PBlock *pb, backend *be, const Slapi_DN *sdn, int lock, back_txn *txn, int flags);
static struct backentry * find_entry_internal(Slapi_PBlock *pb, Slapi_Backend *be, const entry_address *addr, int lock, back_txn *txn, int flags);
/* The flags take these values */
#define FE_TOMBSTONE_INCLUDED TOMBSTONE_INCLUDED /* :1 defined in back-ldbm.h */
#define FE_REALLY_INTERNAL 0x2

int
check_entry_for_referral(Slapi_PBlock *pb, Slapi_Entry *entry, char *matched, const char *callingfn) /* JCM - Move somewhere more appropriate */
{
	int rc=0, i=0, numValues=0;
	Slapi_Attr *attr;
	Slapi_Value *val=NULL;	
	struct berval **refscopy=NULL;
	struct berval **url=NULL;

	/* if the entry is a referral send the referral */
	if ( slapi_entry_attr_find( entry, "ref", &attr ) )
	{
		// ref attribute not found
		goto out;
	}

	slapi_attr_get_numvalues(attr, &numValues );
	if(numValues == 0) {
		// ref attribute is empty
		goto out;
	}

	url=(struct berval **) slapi_ch_malloc((numValues + 1) * sizeof(struct berval*));
	if (!url) {
		LDAPDebug( LDAP_DEBUG_ANY,
			"check_entry_for_referral: Out of memory\n",
			0, 0, 0);
		goto out;
	}

	for (i = slapi_attr_first_value(attr, &val); i != -1;
	     i = slapi_attr_next_value(attr, i, &val)) {
		url[i]=(struct berval*)slapi_value_get_berval(val);
	}
	url[numValues]=NULL;		

	refscopy = ref_adjust( pb, url, slapi_entry_get_sdn(entry), 0 ); /* JCM - What's this PBlock* for? */
	slapi_send_ldap_result( pb, LDAP_REFERRAL, matched, NULL, 0, refscopy );
	rc= 1;

	LDAPDebug( LDAP_DEBUG_TRACE,
		"<= %s sent referral to (%s) for (%s)\n",
		callingfn,
		refscopy ? refscopy[0]->bv_val : "",
		slapi_entry_get_dn(entry));
out:
	if ( refscopy != NULL )
	{
		ber_bvecfree( refscopy );
	}
	if( url != NULL) {
		slapi_ch_free( (void **)&url );	
	}
	return rc;
}

static struct backentry *
find_entry_internal_dn(
	Slapi_PBlock	*pb,
    backend			*be,
    const Slapi_DN *sdn,
    int				lock,
	back_txn		*txn,
	int				flags
)
{ 
	struct backentry *e;
	int	managedsait = 0;
	int	err;
	ldbm_instance *inst = (ldbm_instance *) be->be_instance_info;
	size_t tries = 0;

	/* get the managedsait ldap message control */
	slapi_pblock_get( pb, SLAPI_MANAGEDSAIT, &managedsait );

	while ( (tries < LDBM_CACHE_RETRY_COUNT) && 
	        (e = dn2entry_ext( be, sdn, txn, flags & TOMBSTONE_INCLUDED, &err ))
	        != NULL )
	{
		/*
		 * we found the entry. if the managedsait control is set,
		 * we return the entry. if managedsait is not set, we check
		 * for the presence of a ref attribute, returning to the
		 * client a referral to the ref'ed entry if a ref is present,
		 * returning the entry to the caller if not.
		 */
		if ( !managedsait && !(flags & FE_REALLY_INTERNAL)) {
			/* see if the entry is a referral */
			if(check_entry_for_referral(pb, e->ep_entry, NULL, "find_entry_internal_dn"))
			{
				CACHE_RETURN( &inst->inst_cache, &e );
				return( NULL );
			}
		}

		/*
		 * we'd like to return the entry. lock it if requested,
		 * retrying if necessary.
		 */

		/* wait for entry modify lock */
		if ( !lock || cache_lock_entry( &inst->inst_cache, e ) == 0 ) {
			LDAPDebug( LDAP_DEBUG_TRACE,
			    "<= find_entry_internal_dn found (%s)\n", slapi_sdn_get_dn(sdn), 0, 0 );
			return( e );
		}
		/*
		 * this entry has been deleted - see if it was actually
		 * replaced with a new copy, and try the whole thing again.
		 */
		LDAPDebug( LDAP_DEBUG_ARGS,
		    "   find_entry_internal_dn retrying (%s)\n", slapi_sdn_get_dn(sdn), 0, 0 );
		CACHE_RETURN( &inst->inst_cache, &e );
		tries++;
	}
	if (tries >= LDBM_CACHE_RETRY_COUNT) {
		LDAPDebug( LDAP_DEBUG_ANY,"find_entry_internal_dn retry count exceeded (%s)\n", slapi_sdn_get_dn(sdn), 0, 0 );
	}
	/*
	 * there is no such entry in this server. see how far we
	 * can match, and check if that entry contains a referral.
	 * if it does and managedsait is not set, we return the
	 * referral to the client. if it doesn't, or managedsait
	 * is set, we return no such object.
	 */
	if (!(flags & FE_REALLY_INTERNAL)) {
		struct backentry *me;
		Slapi_DN ancestorsdn;
		slapi_sdn_init(&ancestorsdn);
		me= dn2ancestor(pb->pb_backend,sdn,&ancestorsdn,txn,&err);
		if ( !managedsait && me != NULL ) {
			/* if the entry is a referral send the referral */
			if(check_entry_for_referral(pb, me->ep_entry, (char*)slapi_sdn_get_dn(&ancestorsdn), "find_entry_internal_dn"))
			{
				CACHE_RETURN( &inst->inst_cache, &me );
				slapi_sdn_done(&ancestorsdn);
				return( NULL );
			}
			/* else fall through to no such object */
		}

		/* entry not found */
		slapi_send_ldap_result( pb, ( 0 == err || DB_NOTFOUND == err ) ?
			LDAP_NO_SUCH_OBJECT : ( LDAP_INVALID_DN_SYNTAX == err ) ?
			LDAP_INVALID_DN_SYNTAX : LDAP_OPERATIONS_ERROR,
			(char*)slapi_sdn_get_dn(&ancestorsdn), NULL, 0, NULL );
		slapi_sdn_done(&ancestorsdn);
		CACHE_RETURN( &inst->inst_cache, &me );
	}

	LDAPDebug( LDAP_DEBUG_TRACE, "<= find_entry_internal_dn not found (%s)\n",
	    slapi_sdn_get_dn(sdn), 0, 0 );
	return( NULL );
}

/* Note that this function does not issue any referals.
   It should only be called in case of 5.0 replicated operation
   which should not be referred.
 */
static struct backentry *
find_entry_internal_uniqueid(
	Slapi_PBlock	*pb,
    backend *be,
	const char 			*uniqueid,
    int				lock,
	back_txn		*txn
)
{
	ldbm_instance *inst = (ldbm_instance *) be->be_instance_info;
	struct backentry	*e;
	int			err;
	size_t tries = 0;

	while ( (tries < LDBM_CACHE_RETRY_COUNT) && 
			(e = uniqueid2entry(be, uniqueid, txn, &err ))
	    != NULL ) {

		/*
		 * we'd like to return the entry. lock it if requested,
		 * retrying if necessary.
		 */

		/* wait for entry modify lock */
		if ( !lock || cache_lock_entry( &inst->inst_cache, e ) == 0 ) {
			LDAPDebug( LDAP_DEBUG_TRACE,
			    "<= find_entry_internal_uniqueid found; uniqueid = (%s)\n", 
			uniqueid, 0, 0 );
			return( e );
		}
		/*
		 * this entry has been deleted - see if it was actually
		 * replaced with a new copy, and try the whole thing again.
		 */
		LDAPDebug( LDAP_DEBUG_ARGS,
			"   find_entry_internal_uniqueid retrying; uniqueid = (%s)\n", 
			uniqueid, 0, 0 );
		CACHE_RETURN( &inst->inst_cache, &e );
		tries++;
	}
	if (tries >= LDBM_CACHE_RETRY_COUNT) {
		LDAPDebug( LDAP_DEBUG_ANY,
			"find_entry_internal_uniqueid retry count exceeded; uniqueid = (%s)\n", 
			uniqueid , 0, 0 );
	}

	/* entry not found */
	slapi_send_ldap_result( pb, ( 0 == err || DB_NOTFOUND == err ) ?
		LDAP_NO_SUCH_OBJECT : LDAP_OPERATIONS_ERROR, NULL /* matched */, NULL,
		0, NULL );
	LDAPDebug( LDAP_DEBUG_TRACE, 
		"<= find_entry_internal_uniqueid not found; uniqueid = (%s)\n",
	    uniqueid, 0, 0 );
	return( NULL );
}

static struct backentry *
find_entry_internal(
    Slapi_PBlock		*pb,
    Slapi_Backend *be,
    const entry_address *addr,
    int			lock,
	back_txn *txn,
	int flags
)
{
	/* check if we should search based on uniqueid or dn */
	if (addr->uniqueid!=NULL)
	{
		LDAPDebug( LDAP_DEBUG_TRACE, "=> find_entry_internal (uniqueid=%s) lock %d\n",
		    addr->uniqueid, lock, 0 );
		return (find_entry_internal_uniqueid (pb, be, addr->uniqueid, lock, txn));
	}
	else
	{
		struct backentry *entry = NULL;

		LDAPDebug( LDAP_DEBUG_TRACE, "=> find_entry_internal (dn=%s) lock %d\n",
		           slapi_sdn_get_dn(addr->sdn), lock, 0 );
		if (addr->sdn) {
			entry = find_entry_internal_dn (pb, be, addr->sdn, 
			                                lock, txn, flags);
		} else {
			LDAPDebug0Args( LDAP_DEBUG_ANY,
			                "find_entry_internal: Null target dn\n" );
		}

		LDAPDebug0Args( LDAP_DEBUG_TRACE, "<= find_entry_internal\n" );
		return entry;
	}
}

struct backentry *
find_entry(
    Slapi_PBlock		*pb,
    Slapi_Backend *be,
    const entry_address *addr,
	back_txn *txn
)
{
	return( find_entry_internal( pb, be, addr, 0/*!lock*/, txn, 0/*flags*/ ) );
}

struct backentry *
find_entry2modify(
    Slapi_PBlock		*pb,
    Slapi_Backend *be,
    const entry_address *addr,
	back_txn *txn
)
{
	return( find_entry_internal( pb, be, addr, 1/*lock*/, txn, 0/*flags*/ ) );
}

/* New routines which do not do any referral stuff.
   Call these if all you want to do is get pointer to an entry
   and certainly do not want any side-effects relating to client ! */

struct backentry *
find_entry_only(
    Slapi_PBlock		*pb,
    Slapi_Backend *be,
    const entry_address *addr,
	back_txn *txn
)
{
	return( find_entry_internal( pb, be, addr, 0/*!lock*/, txn, FE_REALLY_INTERNAL ) );
}

struct backentry *
find_entry2modify_only(
    Slapi_PBlock		*pb,
    Slapi_Backend *be,
    const entry_address *addr,
    back_txn *txn
)
{
	return( find_entry_internal( pb, be, addr, 1/*lock*/, txn, FE_REALLY_INTERNAL ) );
}

struct backentry *
find_entry2modify_only_ext(
    Slapi_PBlock		*pb,
    Slapi_Backend *be,
    const entry_address *addr,
    int flags,
    back_txn *txn

)
{
	return( find_entry_internal( pb, be, addr, 1/*lock*/, txn, 
		                         FE_REALLY_INTERNAL | flags ));
}
