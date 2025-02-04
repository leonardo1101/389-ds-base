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

/*
 * ptbind.c - LDAP bind-related code for Pass Through Authentication
 *
 */

#include "passthru.h"

static int
passthru_simple_bind_once_s( PassThruServer *srvr, const char *dn,
	struct berval *creds, LDAPControl **reqctrls, int *lderrnop,
	char **matcheddnp, char **errmsgp, struct berval ***refurlsp,
	LDAPControl ***resctrlsp );


/*
 * Attempt to chain a bind request off to "srvr." We return an LDAP error
 * code that indicates whether we successfully got a response from the
 * other server or not.  If we succeed, we return LDAP_SUCCESS and *lderrnop
 * is set to the result code from the remote server.
 *
 * Note that in the face of "ldap server down" or "ldap connect failed" errors
 * we make up to "tries" attempts to bind to the remote server.  Since we
 * are only interested in recovering silently when the remote server is up
 * but decided to close our connection, we retry without pausing between
 * attempts.
 */
int
passthru_simple_bind_s( Slapi_PBlock *pb, PassThruServer *srvr, int tries,
	const char *dn, struct berval *creds, LDAPControl **reqctrls, int *lderrnop,
	char **matcheddnp, char **errmsgp, struct berval ***refurlsp,
	LDAPControl ***resctrlsp )
{
    int		rc;

    PASSTHRU_ASSERT( srvr != NULL );
    PASSTHRU_ASSERT( tries > 0 );
    PASSTHRU_ASSERT( creds != NULL );
    PASSTHRU_ASSERT( lderrnop != NULL );
    PASSTHRU_ASSERT( refurlsp != NULL );

    do {
	/*
	 * check to see if operation has been abandoned...
	 */
	if ( slapi_op_abandoned( pb )) {
	    slapi_log_error( SLAPI_LOG_PLUGIN, PASSTHRU_PLUGIN_SUBSYSTEM,
		    "operation abandoned\n" );
	    rc = LDAP_USER_CANCELLED;
	} else {
	    rc = passthru_simple_bind_once_s( srvr, dn, creds, reqctrls,
		    lderrnop, matcheddnp, errmsgp, refurlsp, resctrlsp );
	}
    } while ( PASSTHRU_LDAP_CONN_ERROR( rc ) && --tries > 0 );

    return( rc );
}


/*
 * like passthru_simple_bind_s() but only makes one attempt.
 */
static int
passthru_simple_bind_once_s( PassThruServer *srvr, const char *dn,
	struct berval *creds, LDAPControl **reqctrls, int *lderrnop,
	char **matcheddnp, char **errmsgp, struct berval ***refurlsp,
	LDAPControl ***resctrlsp )
{
    int			rc, msgid;
    char		**referrals;
    struct timeval	tv, *timeout;
    LDAPMessage		*result;
    LDAP		*ld;

    /*
     * Grab an LDAP connection to use for this bind.
     */
    ld = NULL;
    if (( rc = passthru_get_connection( srvr, &ld )) != LDAP_SUCCESS ) {
	goto release_and_return;
    }

    /*
     * Send the bind operation (need to retry on LDAP_SERVER_DOWN)
     */
    if (( rc = ldap_sasl_bind( ld, dn, LDAP_SASL_SIMPLE, creds, reqctrls,
		NULL, &msgid )) != LDAP_SUCCESS ) {
	goto release_and_return;
    }

    /*
     * determine timeout value (how long we will wait for a response)
     * if timeout is NULL or zero'd, we wait indefinitely.
     */
    if ( srvr->ptsrvr_timeout == NULL || ( srvr->ptsrvr_timeout->tv_sec == 0
	    && srvr->ptsrvr_timeout->tv_usec == 0 )) {
	timeout = NULL;
    } else {
	tv = *srvr->ptsrvr_timeout;	/* struct copy */
	timeout = &tv;
    }

    /*
     * Wait for a result.
     */
    rc = ldap_result( ld, msgid, 1, timeout, &result );

    /*
     * Interpret the result.
     */
    if ( rc == 0 ) {		/* timeout */
	/*
	 * Timed out waiting for a reply from the server.
	 */
	rc = LDAP_TIMEOUT;
    } else if ( rc < 0 ) {
	/*
	 * Some other error occurred (no result received).
	 */
	rc = slapi_ldap_get_lderrno( ld, matcheddnp, errmsgp );
    } else {
	/*
	 * Got a result from remote server -- parse it.
	 */
	rc = ldap_parse_result( ld, result, lderrnop, matcheddnp, errmsgp,
		&referrals, resctrlsp, 1 );
	if ( referrals != NULL ) {
	    *refurlsp = passthru_strs2bervals( referrals );
	    slapi_ldap_value_free( referrals );
	}
    }


release_and_return:
    if ( ld != NULL ) {
	passthru_release_connection( srvr, ld, PASSTHRU_LDAP_CONN_ERROR( rc ));
    }

    return( rc );
}
