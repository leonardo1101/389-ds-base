/** BEGIN COPYRIGHT BLOCK
 * Copyright (C) 2001 Sun Microsystems, Inc. Used by permission.
 * Copyright (C) 2010 Red Hat, Inc.
 * All rights reserved.
 *
 * License: GPL (version 3 or any later version).
 * See LICENSE for details. 
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ldap.h>

#include "slap.h"

#define BEGIN do {
#define END   } while(0);

/* ------------------------------------------------------------
 * LDAPProxyAuth
 *
 * ProxyAuthControl ::= SEQUENCE {
 *   authorizationDN LDAPDN
 * } 
 */
struct LDAPProxyAuth
{
  char *auth_dn;
};
typedef struct LDAPProxyAuth LDAPProxyAuth;

/*
 * delete_LDAPProxyAuth
 */
static void
delete_LDAPProxyAuth(LDAPProxyAuth *spec)
{
  if (!spec) return;

  slapi_ch_free((void**)&spec->auth_dn);
  slapi_ch_free((void**)&spec);
}

/*
 * parse_LDAPProxyAuth
 *
 * Parse a BER encoded value into the compoents of the LDAP ProxyAuth control.
 * The 'version' parameter should be 1 or 2.
 *
 * Returns an LDAP error code (LDAP_SUCCESS if all goes well) and sets
 * *errtextp if appropriate.
 */
static int
parse_LDAPProxyAuth(struct berval *spec_ber, int version, char **errtextp,
		LDAPProxyAuth **out)
{
  int lderr = LDAP_OPERATIONS_ERROR;	/* pessimistic */
  LDAPProxyAuth *spec = NULL;
  BerElement *ber = NULL;
  char *errstring = "unable to parse proxied authorization control";
  Slapi_DN *sdn = NULL;

  BEGIN
	ber_tag_t tag;

	if ( version != 1 && version != 2 ) {
		break;
	}

	if (!BV_HAS_DATA(spec_ber)) {
		break;
	}

	/* create_LDAPProxyAuth */
	spec = (LDAPProxyAuth*)slapi_ch_calloc(1,sizeof (LDAPProxyAuth));
	if (!spec) {
		break;
	}

	if (version == 2 && (spec_ber->bv_val[0] != CHAR_OCTETSTRING)) {
		/* This doesn't start with an octet string, so just use the actual value */
		spec->auth_dn = slapi_ch_strdup(spec_ber->bv_val);
	} else {
		ber = ber_init(spec_ber);
		if (!ber) {
			break;
		}

		if ( version == 1 ) {
			tag = ber_scanf(ber, "{a}", &spec->auth_dn);
		} else {
			tag = ber_scanf(ber, "a", &spec->auth_dn);
		}
		if (tag == LBER_ERROR) {
			lderr = LDAP_PROTOCOL_ERROR;
			break;
		}
	}
	/*
	 * In version 2 of the control, the control value is actually an
	 * authorization ID (see section 9 of RFC 2829).  We only support
	 * the "dnAuthzId" flavor, which looks like "dn:<DN>" where <DN> is
	 * an actual DN, e.g., "dn:uid=bjensen,dc=example,dc=com".  So we
	 * need to strip off the dn: if present and reject the operation if
	 * not.
	 */
	if (2 == version) {
		if ( NULL == spec->auth_dn || strlen( spec->auth_dn ) < 3 ||
				strncmp( "dn:", spec->auth_dn, 3 ) != 0 ) {
			lderr = LDAP_INSUFFICIENT_ACCESS;	/* per Proxied Auth. I-D */
			errstring = "proxied authorization id must be a DN (dn:...)";
			break;
		}
		/* memmove is safe for overlapping copy */
		memmove ( spec->auth_dn, spec->auth_dn + 3, strlen(spec->auth_dn) - 2);/* 1 for '\0' */
	}

	lderr = LDAP_SUCCESS;	/* got it! */
	sdn = slapi_sdn_new_dn_passin(spec->auth_dn);
	spec->auth_dn = slapi_ch_strdup(slapi_sdn_get_ndn(sdn));
	slapi_sdn_free(&sdn);
	if (NULL == spec->auth_dn) {
		lderr = LDAP_INVALID_SYNTAX;
	}
  END

  /* Cleanup */
  if (ber) ber_free(ber, 1);

  if ( LDAP_SUCCESS != lderr)
  {
    if (spec) delete_LDAPProxyAuth(spec);
	spec = 0;
	if ( NULL != errtextp ) {
		*errtextp = errstring;
	}
  }

  *out = spec;

  return lderr;
}

/*
 * proxyauth_dn - find the users DN in the proxyauth control if it is
 *   present. The return value has been malloced for you.
 *
 * Returns an LDAP error code.  If anything than LDAP_SUCCESS is returned,
 * the error should be returned to the client.  LDAP_SUCCESS is always
 * returned if the proxy auth control is not present or not critical.
 */
int
proxyauth_get_dn( Slapi_PBlock *pb, char **proxydnp, char **errtextp )
{
	char *dn = 0;
	LDAPProxyAuth *spec = 0;
	int rv, lderr = LDAP_SUCCESS;	/* optimistic */

	BEGIN
		struct berval *spec_ber;
		LDAPControl **controls;
		int present;
		int critical;
		int	version = 1;

		rv = slapi_pblock_get( pb, SLAPI_REQCONTROLS, &controls );
		if (rv) break;

		present = slapi_control_present( controls, LDAP_CONTROL_PROXYAUTH,
			&spec_ber, &critical );
		if (!present) {
			present = slapi_control_present( controls, LDAP_CONTROL_PROXIEDAUTH,
				&spec_ber, &critical );
			if (!present) break;
			version = 2;
			/*
			 * Note the according to the Proxied Authorization I-D, the
			 * control is always supposed to be marked critical by the
			 * client.  If it is not, we return a protocolError.
			 */
			if ( !critical ) {
				lderr = LDAP_PROTOCOL_ERROR;
				if ( NULL != errtextp ) {
					*errtextp = "proxy control must be marked critical";
				}
				break;
			}
		}

		rv = parse_LDAPProxyAuth(spec_ber, version, errtextp, &spec);
		if (LDAP_SUCCESS != rv) {
			if ( critical ) {
				lderr = LDAP_UNAVAILABLE_CRITICAL_EXTENSION;
			} else {
				lderr = rv;
			}
			break;
		}

		dn = slapi_ch_strdup(spec->auth_dn);
		if (slapi_dn_isroot(dn) ) {
			if (critical)
				lderr = LDAP_UNAVAILABLE_CRITICAL_EXTENSION;
			else
				lderr = LDAP_UNWILLING_TO_PERFORM;
			*errtextp = "Proxy dn should not be rootdn";
			break;
			
		}
	END

	if (spec) delete_LDAPProxyAuth(spec);

	if ( NULL != proxydnp ) {
		*proxydnp = dn;
	} else {
		slapi_ch_free_string(&dn);
	}

	return lderr;
}

