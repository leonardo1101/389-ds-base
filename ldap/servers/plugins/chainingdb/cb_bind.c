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

#include "cb.h"

static void
cb_free_bervals( struct berval **bvs );


static int
cb_sasl_bind_once_s( cb_conn_pool *pool, const char *dn, ber_tag_t method, 
                     char * mechanism, struct berval *creds, 
                     LDAPControl **reqctrls, char **matcheddnp, 
                     char **errmsgp, struct berval ***refurlsp,
                     LDAPControl ***resctrlsp , int * status);

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

static int
cb_sasl_bind_s(Slapi_PBlock * pb, cb_conn_pool *pool, int tries,
               const char *dn, ber_tag_t method, char * mechanism, 
               struct berval *creds, LDAPControl **reqctrls,
               char **matcheddnp, char **errmsgp, struct berval ***refurlsp,
               LDAPControl ***resctrlsp ,int *status)
{
    int         rc;
 
    do {
         /* check to see if operation has been abandoned...*/

    if (LDAP_AUTH_SIMPLE!=method)
        return LDAP_AUTH_METHOD_NOT_SUPPORTED;

        if ( slapi_op_abandoned( pb )) {
            rc = LDAP_USER_CANCELLED;
        } else {
            rc = cb_sasl_bind_once_s( pool, dn, method, mechanism, creds, reqctrls,
                     matcheddnp, errmsgp, refurlsp, resctrlsp ,status);
        }
    } while ( CB_LDAP_CONN_ERROR( rc ) && --tries > 0 );
       
    return( rc );
}

static int
cb_sasl_bind_once_s( cb_conn_pool *pool, const char *dn, ber_tag_t method, 
                     char * mechanism, struct berval *creds, 
                     LDAPControl **reqctrls, char **matcheddnp, 
                     char **errmsgp, struct berval ***refurlsp,
                     LDAPControl ***resctrlsp , int * status )
{
    int                 rc, msgid;
    char                **referrals;
    struct timeval      timeout_copy, *timeout;
    LDAPMessage         *result=NULL;
    LDAP                *ld=NULL;
    char 		*cnxerrbuf=NULL;
    cb_outgoing_conn	*cnx;
    int version=LDAP_VERSION3;
	
    /* Grab an LDAP connection to use for this bind. */

    slapi_rwlock_rdlock(pool->rwl_config_lock);
    timeout_copy.tv_sec = pool->conn.bind_timeout.tv_sec;
    timeout_copy.tv_usec = pool->conn.bind_timeout.tv_usec;
    slapi_rwlock_unlock(pool->rwl_config_lock);

	rc = cb_get_connection(pool, &ld, &cnx, NULL, &cnxerrbuf);
	if (LDAP_SUCCESS != rc) {
		static int warned_get_conn = 0;
		if (!warned_get_conn) {
			slapi_log_error(SLAPI_LOG_FATAL, CB_PLUGIN_SUBSYSTEM,
			                "cb_get_connection failed (%d) %s\n",
			                rc, ldap_err2string(rc));
			warned_get_conn = 1;
		}
		*errmsgp = cnxerrbuf;
		goto release_and_return;
	}
       
    /* Send the bind operation (need to retry on LDAP_SERVER_DOWN) */
    
    ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version );

    if (( rc = ldap_sasl_bind( ld, dn, LDAP_SASL_SIMPLE, creds, reqctrls,
                NULL, &msgid )) != LDAP_SUCCESS ) {
        goto release_and_return;
    }

	/* XXXSD what is the exact semantics of bind_to ? it is used to get a
	connection handle and later to bind ==> bind op may last 2*bind_to
	from the user point of view 
	confusion comes from teh fact that bind to is used 2for 3 differnt thinks,	
	*/

    /*
     * determine timeout value (how long we will wait for a response)
     * if timeout is zero'd, we wait indefinitely.
     */
    if ( timeout_copy.tv_sec == 0 && timeout_copy.tv_usec == 0 ) {
        timeout = NULL;
    } else {
	timeout = &timeout_copy;
    }
       
    /*
     * Wait for a result.
     */
    rc = ldap_result( ld, msgid, 1, timeout, &result );
 
    /*
     * Interpret the result.
     */

   if ( rc == 0 ) {            /* timeout */
        /*
         * Timed out waiting for a reply from the server.
         */
        rc = LDAP_TIMEOUT;
    } else if ( rc < 0 ) {

        /* Some other error occurred (no result received). */
	char * matcheddnp2, * errmsgp2;
	matcheddnp2=errmsgp2=NULL;

	rc = slapi_ldap_get_lderrno( ld, &matcheddnp2, &errmsgp2 );

	/* Need to allocate errmsgs */
	if (matcheddnp2)
		*matcheddnp=slapi_ch_strdup(matcheddnp2);
	if (errmsgp2)
		*errmsgp=slapi_ch_strdup(errmsgp2);
	
	if ( LDAP_SUCCESS != rc )  {
		static int warned_bind_once = 0;
		if (!warned_bind_once) {
			int hasmatched = (matcheddnp && *matcheddnp && (**matcheddnp != '\0'));
			slapi_log_error(SLAPI_LOG_FATAL, CB_PLUGIN_SUBSYSTEM,
			                "cb_sasl_bind_once_s failed (%s%s%s)\n",
			                hasmatched ? *matcheddnp : "", 
			                hasmatched ? ": " : "",
			                ldap_err2string(rc));
			warned_bind_once = 1;
		}
	}
    } else {

        /* Got a result from remote server -- parse it.*/

	char * matcheddnp2, * errmsgp2;
	matcheddnp2=errmsgp2=NULL;
	*resctrlsp=NULL;
        rc = ldap_parse_result( ld, result, status, &matcheddnp2, &errmsgp2,
                &referrals, resctrlsp, 1 );
        if ( referrals != NULL ) {
            *refurlsp = referrals2berval( referrals );
            slapi_ldap_value_free( referrals );
        }
	/* realloc matcheddn & errmsg because the mem alloc model */
	/* may differ from malloc				  */
	if (matcheddnp2) {
		*matcheddnp=slapi_ch_strdup(matcheddnp2);
		ldap_memfree(matcheddnp2);
	}
	if (errmsgp2) {
		*errmsgp=slapi_ch_strdup(errmsgp2);
		ldap_memfree(errmsgp2);
	}

    }

release_and_return:
    if ( ld != NULL ) {
        cb_release_op_connection( pool, ld, CB_LDAP_CONN_ERROR( rc ));
    }
       
    return( rc );
}

int
chainingdb_bind( Slapi_PBlock *pb )
{
	cb_backend_instance *cb;
	Slapi_Backend *be;
	struct berval *creds = NULL, **urls = NULL;
	const char *dn = NULL;
	Slapi_DN *sdn = NULL;
	Slapi_DN *mysdn = NULL;
	char *matcheddn = NULL, *errmsg = NULL;
	LDAPControl **reqctrls = NULL, **resctrls = NULL, **ctrls = NULL;
	char *mechanism = NULL;
	int status=LDAP_SUCCESS;
	int allocated_errmsg = 0;
	int rc = LDAP_SUCCESS;
	int freectrls = 1;
	int bind_retry;
	ber_tag_t method;
	
	if ( LDAP_SUCCESS != (rc = cb_forward_operation(pb) )) {
		cb_send_ldap_result( pb, rc, NULL, "Chaining forbidden", 0, NULL );
		return SLAPI_BIND_FAIL;
	}

	/* don't add proxy auth control. use this call to check for supported   */
	/* controls only.							*/
	if ( LDAP_SUCCESS != ( rc = cb_update_controls( pb, NULL, &ctrls, 0 )) ) {
		cb_send_ldap_result( pb, rc, NULL, NULL, 0, NULL );
		if (ctrls)
			ldap_controls_free(ctrls);
		return SLAPI_BIND_FAIL;
	}
	if (ctrls)
		ldap_controls_free(ctrls);

	slapi_pblock_get( pb, SLAPI_BACKEND, &be );
	slapi_pblock_get( pb, SLAPI_BIND_TARGET_SDN, &sdn );
	slapi_pblock_get( pb, SLAPI_BIND_METHOD, &method );
	slapi_pblock_get( pb, SLAPI_BIND_SASLMECHANISM, &mechanism);
	slapi_pblock_get( pb, SLAPI_BIND_CREDENTIALS, &creds );
	if (NULL == creds) {
		cb_send_ldap_result( pb, rc, NULL, "No credentials", 0, NULL );
		return SLAPI_BIND_FAIL;
	}
	slapi_pblock_get( pb, SLAPI_REQCONTROLS, &reqctrls );
	cb = cb_get_instance(be);

	if ( NULL == sdn ) {
		sdn = mysdn = slapi_sdn_new_ndn_byval("");
	}
	dn = slapi_sdn_get_ndn(sdn);

	/* always allow noauth simple binds */
	if ((method == LDAP_AUTH_SIMPLE) && (creds->bv_len == 0)) {
		slapi_sdn_free(&mysdn);
		return( SLAPI_BIND_ANONYMOUS );
	}

	cb_update_monitor_info(pb,cb,SLAPI_OPERATION_BIND);

	/* Check wether the chaining BE is available or not */
	if ( cb_check_availability( cb, pb ) == FARMSERVER_UNAVAILABLE ){
		slapi_sdn_free(&mysdn);
		return -1;
	}

	slapi_rwlock_rdlock(cb->rwl_config_lock);
	bind_retry=cb->bind_retry;
	slapi_rwlock_unlock(cb->rwl_config_lock);

	rc = cb_sasl_bind_s(pb, cb->bind_pool, bind_retry, dn, method, 
	                    mechanism, creds, reqctrls, &matcheddn, &errmsg, 
	                    &urls, &resctrls, &status);
	if ( LDAP_SUCCESS == rc ) {
		rc = status;
		allocated_errmsg = 1;
	} else if ( LDAP_USER_CANCELLED != rc ) {
		slapi_ch_free_string(&errmsg);
		errmsg = ldap_err2string( rc );
		if (rc == LDAP_TIMEOUT) {
			cb_ping_farm(cb,NULL,0);
		}
		rc = LDAP_OPERATIONS_ERROR;
	} else {
		allocated_errmsg = 1;
	}

	if ( rc != LDAP_USER_CANCELLED ) {  /* not abandoned */
		if ( resctrls != NULL ) {
			slapi_pblock_set( pb, SLAPI_RESCONTROLS, resctrls );
			freectrls = 0;
		}

		if ( rc != LDAP_SUCCESS ) {
			cb_send_ldap_result( pb, rc, matcheddn, errmsg, 0, urls );
		}
	}

	if ( urls != NULL ) {
		cb_free_bervals( urls );
	}
	if ( freectrls && ( resctrls != NULL )) {
		ldap_controls_free( resctrls );
	}
	slapi_ch_free_string(&matcheddn);
	if ( allocated_errmsg ) {
		slapi_ch_free_string(&errmsg);
	}

	slapi_sdn_free(&mysdn);
	return ((rc == LDAP_SUCCESS ) ? SLAPI_BIND_SUCCESS : SLAPI_BIND_FAIL );
}

static void
cb_free_bervals( struct berval **bvs )
{
    int         i;

    if ( bvs != NULL ) {
        for ( i = 0; bvs[ i ] != NULL; ++i ) {
            slapi_ch_free( (void **)&bvs[ i ] );
        }
    }    
    slapi_ch_free( (void **)&bvs );
}

