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

/*
 * Perform a compare operation
 *
 * Returns:
 *   0  - success
 *   <0 - fail
 *
 */

int
chaining_back_compare ( Slapi_PBlock *pb )
{

	Slapi_Backend		* be;
	cb_backend_instance 	*cb=NULL;
        struct berval           *bval=NULL;
	LDAPControl 		**ctrls, **serverctrls;
	int 			rc,parse_rc,msgid,i,checkacl;
	LDAP 			*ld=NULL;
	char         		**referrals=NULL;
	LDAPMessage		* res;
	char 			*type, *matched_msg, *error_msg;
	const char 		*dn = NULL;
	Slapi_DN		*sdn = NULL; 
	char 			*cnxerrbuf=NULL;
	time_t 			endtime = 0;
	cb_outgoing_conn	*cnx;

	if ( LDAP_SUCCESS != (rc=cb_forward_operation(pb) )) {
		cb_send_ldap_result( pb, rc, NULL, "Chaining forbidden", 0, NULL );
		return -1;
	}

	slapi_pblock_get( pb, SLAPI_BACKEND, &be );
	cb = cb_get_instance(be);

	cb_update_monitor_info(pb,cb,SLAPI_OPERATION_COMPARE);

	/* Check wether the chaining BE is available or not */
	if ( cb_check_availability( cb, pb ) == FARMSERVER_UNAVAILABLE ){
		return -1;
	}

	slapi_pblock_get( pb, SLAPI_COMPARE_TYPE, &type );
	slapi_pblock_get( pb, SLAPI_COMPARE_VALUE, &bval );
	slapi_pblock_get( pb, SLAPI_COMPARE_TARGET_SDN, &sdn );
	if (NULL == sdn) {
		cb_send_ldap_result(pb, LDAP_INVALID_DN_SYNTAX, NULL, "Null target DN", 0, NULL );
		return -1;
	}
	dn = slapi_sdn_get_dn(sdn);

	/* 
	 * Check local acls
	 * No need to lock the config to access cb->local_acl
	 */

	checkacl=cb->local_acl && !cb->associated_be_is_disabled;

	if (checkacl) {
		char * errbuf=NULL;
		Slapi_Entry *te = slapi_entry_alloc();
		slapi_entry_set_sdn(te, sdn); /* sdn: copied */
		rc = cb_access_allowed (pb, te, type, bval, SLAPI_ACL_COMPARE,&errbuf);
		slapi_entry_free(te);

		if ( rc != LDAP_SUCCESS ) {
			cb_send_ldap_result( pb, rc, NULL, errbuf, 0, NULL );
			slapi_ch_free((void **) &errbuf);
			return 1;
		}
	}

	/*
	 * Grab a connection handle
	 */
	rc = cb_get_connection(cb->pool, &ld, &cnx, NULL, &cnxerrbuf);
	if (LDAP_SUCCESS != rc) {
		static int warned_get_conn = 0;
		if (!warned_get_conn) {
			slapi_log_error(SLAPI_LOG_FATAL, CB_PLUGIN_SUBSYSTEM,
			                "cb_get_connection failed (%d) %s\n",
			                rc, ldap_err2string(rc));
			warned_get_conn = 1;
		}
		cb_send_ldap_result(pb, LDAP_OPERATIONS_ERROR, NULL, 
		                    cnxerrbuf, 0, NULL);
		slapi_ch_free_string(&cnxerrbuf);
		/* ping the farm. 
		 * If the farm is unreachable, we increment the counter */
		cb_ping_farm(cb, NULL, 0);
		return 1;
	}

 	/*
         * Control management
         */

        if ( (rc = cb_update_controls( pb,ld,&ctrls,CB_UPDATE_CONTROLS_ADDAUTH )) != LDAP_SUCCESS ) {
                cb_send_ldap_result( pb, rc, NULL,NULL, 0, NULL);
                cb_release_op_connection(cb->pool,ld,CB_LDAP_CONN_ERROR(rc));
                return 1;
        }

        if ( slapi_op_abandoned( pb )) { 
                cb_release_op_connection(cb->pool,ld,0);
		if ( NULL != ctrls)
                	ldap_controls_free(ctrls);
                return -1;
        }
 
	/* heart-beat management */
	if (cb->max_idle_time>0)
        	endtime=current_time() + cb->max_idle_time;

	/*
	 * Send LDAP operation to the remote host
	 */

	rc = ldap_compare_ext( ld, dn, type, bval, ctrls, NULL, &msgid );
	if ( NULL != ctrls)
                ldap_controls_free(ctrls);

	if ( rc != LDAP_SUCCESS ) {
                cb_send_ldap_result( pb, LDAP_OPERATIONS_ERROR, NULL,
                        ldap_err2string(rc), 0, NULL);
		cb_release_op_connection(cb->pool,ld,CB_LDAP_CONN_ERROR(rc));
                return 1;
	}

	while ( 1 ) {

                if (cb_check_forward_abandon(cb,pb,ld,msgid)) {
			return -1;
		}

		/* No need to lock the config to access cb->abandon_timeout */
   		rc = ldap_result( ld, msgid, 0, &cb->abandon_timeout, &res );
   		switch ( rc ) {
   		case -1:
                	cb_send_ldap_result( pb, LDAP_OPERATIONS_ERROR, NULL,
                        	ldap_err2string(rc), 0, NULL);
			cb_release_op_connection(cb->pool,ld,CB_LDAP_CONN_ERROR(rc));
			if (res)
				ldap_msgfree(res);
                	return 1;
		case 0:
			if ((rc=cb_ping_farm(cb,cnx,endtime)) != LDAP_SUCCESS) {

				/* does not respond. give up and return a*/
				/* error to the client.			 */

               			/*cb_send_ldap_result(pb,LDAP_OPERATIONS_ERROR, NULL,
					ldap_err2string(rc), 0, NULL);*/
				cb_send_ldap_result(pb,LDAP_OPERATIONS_ERROR, NULL,     "FARM SERVER TEMPORARY UNAVAILABLE", 0, NULL);
				cb_release_op_connection(cb->pool,ld,CB_LDAP_CONN_ERROR(rc));
				if (res)
					ldap_msgfree(res);
               			return 1;
			}
#ifdef CB_YIELD
                        DS_Sleep(PR_INTERVAL_NO_WAIT);
#endif
                        break;
		default:
			matched_msg=error_msg=NULL;
			parse_rc = ldap_parse_result( ld, res, &rc, &matched_msg, 
			                          &error_msg, &referrals, &serverctrls, 1 );
			if ( parse_rc != LDAP_SUCCESS ) {
				static int warned_parse_rc = 0;
				if (!warned_parse_rc) {
					slapi_log_error( SLAPI_LOG_FATAL, CB_PLUGIN_SUBSYSTEM,
						            "%s%s%s\n", 
						            matched_msg?matched_msg:"",
						            (matched_msg&&(*matched_msg!='\0'))?": ":"",
					                ldap_err2string(parse_rc));
					warned_parse_rc = 1;
				}
				cb_send_ldap_result( pb, LDAP_OPERATIONS_ERROR, NULL,
				                     ENDUSERMSG, 0, NULL );
				cb_release_op_connection(cb->pool,ld,CB_LDAP_CONN_ERROR(parse_rc));
				slapi_ch_free((void **)&matched_msg);
				slapi_ch_free((void **)&error_msg);
				if (serverctrls)
					ldap_controls_free(serverctrls);
				/* jarnou: free referrals */
				if (referrals)
					charray_free(referrals);
				return 1;
			}

			switch ( rc ) {

				case LDAP_COMPARE_TRUE:
				case LDAP_COMPARE_FALSE:
					break;
				default: {
					struct berval ** refs =  referrals2berval(referrals); 

                        		cb_send_ldap_result( pb, rc, matched_msg, error_msg, 0, refs);
					cb_release_op_connection(cb->pool,ld,CB_LDAP_CONN_ERROR(rc));
			       		slapi_ch_free((void **)&matched_msg);
			       		slapi_ch_free((void **)&error_msg);
					if (refs) 
						ber_bvecfree(refs);
					if (referrals) 
						charray_free(referrals);
					if (serverctrls)
	                                	ldap_controls_free(serverctrls);
                			return 1;
				}
			}

			/* Add control response sent by the farm server */

			for (i=0; serverctrls && serverctrls[i];i++)
        			slapi_pblock_set( pb, SLAPI_ADD_RESCONTROL, serverctrls[i]);
			if (serverctrls)
	                	ldap_controls_free(serverctrls);
			/* jarnou: free matched_msg, error_msg, and referrals if necessary */
		       	slapi_ch_free((void **)&matched_msg);
		       	slapi_ch_free((void **)&error_msg);
		       	if (referrals) 
		       		charray_free(referrals);
	
        		cb_send_ldap_result( pb, rc , NULL, NULL, 0, NULL );
			cb_release_op_connection(cb->pool,ld,0);
			return 0;
		}
	}

	/* Never reached */
	/* return 0; */
}
