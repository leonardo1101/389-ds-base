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

#ifndef _LDAPU_ERRORS_H
#define _LDAPU_ERRORS_H

#ifndef NSAPI_PUBLIC
#define NSAPI_PUBLIC 
#endif

#ifdef DBG_PRINT
#include <stdio.h>
#define DBG_PRINT1(x) fprintf(stderr, x)
#define DBG_PRINT2(x,y) fprintf(stderr, x, y)
#define DBG_PRINT3(x,y,z) fprintf(stderr, x, y, z)
#define DBG_PRINT4(x,y,z,a) fprintf(stderr, x, y, z, a)
#else
#define DBG_PRINT1(x) 
#define DBG_PRINT2(x,y) 
#define DBG_PRINT3(x,y,z) 
#define DBG_PRINT4(x,y,z,a) 
#endif

/* Common error codes */
#define LDAPU_ERR_NOT_IMPLEMENTED	     -1000
#define LDAPU_ERR_INTERNAL		     -1001
/* #define LDAPU_SUCCESS	0 */	    /* defined in extcmap.h */
/* #define LDAPU_FAILED		-1 */	    /* defined in extcmap.h */
/* #define LDAPU_CERT_MAP_FUNCTION_FAILED    -2 *//* defined in extcmap.h */
/* #define LDAPU_CERT_VERIFY_FUNCTION_FAILED -3 *//* defined in extcmap.h */
/* #define LDAPU_CERT_VERIFY_FUNCTION_FAILED -4 *//* defined in extcmap.h */
/* #define LDAPU_CERT_MAP_INITFN_FAILED      -5 *//* defined in extcmap.h */

/* Error codes returned by ldapdb.c */
#define LDAPU_ERR_OUT_OF_MEMORY		     -110
#define LDAPU_ERR_URL_INVALID_PREFIX	     -112
#define LDAPU_ERR_URL_NO_BASEDN		     -113
#define LDAPU_ERR_URL_PARSE_FAILED	     -114
    
#define LDAPU_ERR_LDAP_INIT_FAILED	     -120
#define LDAPU_ERR_LCACHE_INIT_FAILED	     -121 
#define LDAPU_ERR_LDAP_SET_OPTION_FAILED     -122 
#define LDAPU_ERR_NO_DEFAULT_CERTDB          -123

/* Errors returned by dbconf.c */
#define LDAPU_ERR_CANNOT_OPEN_FILE	     -141
#define LDAPU_ERR_DBNAME_IS_MISSING	     -142
#define LDAPU_ERR_PROP_IS_MISSING	     -143
#define LDAPU_ERR_DIRECTIVE_IS_MISSING	     -145
#define LDAPU_ERR_NOT_PROPVAL		     -146
#define LDAPU_ATTR_NOT_FOUND		     -147

/* Error codes returned by certmap.c */
#define LDAPU_ERR_NO_ISSUERDN_IN_CERT	     -181
#define LDAPU_ERR_NO_ISSUERDN_IN_CONFIG_FILE -182
#define LDAPU_ERR_CERTMAP_INFO_MISSING	     -183
#define LDAPU_ERR_MALFORMED_SUBJECT_DN	     -184
#define LDAPU_ERR_MAPPED_ENTRY_NOT_FOUND     -185
#define LDAPU_ERR_UNABLE_TO_LOAD_PLUGIN	     -186
#define LDAPU_ERR_MISSING_INIT_FN_IN_LIB     -187
#define LDAPU_ERR_MISSING_INIT_FN_IN_CONFIG  -188
#define LDAPU_ERR_CERT_VERIFY_FAILED	     -189
#define LDAPU_ERR_CERT_VERIFY_NO_CERTS	     -190
#define LDAPU_ERR_MISSING_LIBNAME	     -191
#define LDAPU_ERR_MISSING_INIT_FN_NAME	     -192

#define LDAPU_ERR_EMPTY_LDAP_RESULT	     -193
#define LDAPU_ERR_MULTIPLE_MATCHES	     -194
#define LDAPU_ERR_MISSING_RES_ENTRY	     -195
#define LDAPU_ERR_MISSING_UID_ATTR	     -196
#define LDAPU_ERR_WRONG_ARGS		     -197
#define LDAPU_ERR_RENAME_FILE_FAILED	     -198

#define LDAPU_ERR_MISSING_VERIFYCERT_VAL     -199
#define LDAPU_ERR_CANAME_IS_MISSING	     -200
#define LDAPU_ERR_CAPROP_IS_MISSING	     -201
#define LDAPU_ERR_UNKNOWN_CERT_ATTR	     -202
#define LDAPU_ERR_INVALID_ARGUMENT	     -203
#define LDAPU_ERR_INVALID_SUFFIX	     -204

/* Error codes returned by cert.c */
#define LDAPU_ERR_EXTRACT_SUBJECTDN_FAILED  -300
#define LDAPU_ERR_EXTRACT_ISSUERDN_FAILED   -301
#define LDAPU_ERR_EXTRACT_DERCERT_FAILED    -302

/* Error codes returned by ldapauth.c */
#define LDAPU_ERR_CIRCULAR_GROUPS	    -400
#define LDAPU_ERR_INVALID_STRING	    -401
#define LDAPU_ERR_INVALID_STRING_INDEX	    -402
#define LDAPU_ERR_MISSING_ATTR_VAL	    -403

#ifdef __cplusplus
extern "C" {
#endif

    /* NSAPI_PUBLIC extern char *ldapu_err2string(int err); */

#ifdef __cplusplus
}
#endif

#endif /* LDAPUTIL_LDAPU_H */
