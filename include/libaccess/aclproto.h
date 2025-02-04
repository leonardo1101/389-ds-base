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

#ifndef ACL_PROTO_HEADER
#define ACL_PROTO_HEADER

#ifndef NOINTNSACL
#define INTNSACL
#endif /* !NOINTNSACL */

#ifndef PUBLIC_NSACL_ACLDEF_H
#include "public/nsacl/acldef.h"
#endif /* !PUBLIC_NSACL_ACLDEF_H */

#ifdef INTNSACL

NSPR_BEGIN_EXTERN_C

/*********************************************************************
 *  ACL language and file interfaces
 *********************************************************************/
NSAPI_PUBLIC ACLListHandle_t * ACL_ParseString(NSErr_t *errp, char *buffer);

/*********************************************************************
 *  ACL Expression construction interfaces
 *********************************************************************/
NSAPI_PUBLIC ACLExprHandle_t *ACL_ExprNew(const ACLExprType_t expr_type);
NSAPI_PUBLIC void ACL_ExprDestroy(ACLExprHandle_t *expr);
NSAPI_PUBLIC int ACL_ExprSetPFlags(NSErr_t *errp, ACLExprHandle_t *expr, PFlags_t flags);
NSAPI_PUBLIC int ACL_ExprClearPFlags(NSErr_t *errp, ACLExprHandle_t *expr);
NSAPI_PUBLIC int ACL_ExprTerm(NSErr_t *errp, ACLExprHandle_t *acl_expr, const char *attr_name, CmpOp_t cmp, char *attr_pattern);
NSAPI_PUBLIC int ACL_ExprNot(NSErr_t *errp, ACLExprHandle_t *acl_expr);
NSAPI_PUBLIC int ACL_ExprAnd(NSErr_t *errp, ACLExprHandle_t *acl_expr);
NSAPI_PUBLIC int ACL_ExprOr(NSErr_t *errp, ACLExprHandle_t *acl_expr);
NSAPI_PUBLIC int ACL_ExprAddAuthInfo(ACLExprHandle_t *expr, PList_t auth_info);
NSAPI_PUBLIC int ACL_ExprAddArg(NSErr_t *errp, ACLExprHandle_t *expr, const char *arg);
NSAPI_PUBLIC int ACL_ExprSetDenyWith(NSErr_t *errp, ACLExprHandle_t *expr, char *deny_type, char *deny_response);
NSAPI_PUBLIC int ACL_ExprGetDenyWith(NSErr_t *errp, ACLExprHandle_t *expr, char **deny_type, char **deny_response);

/*********************************************************************
 * ACL manipulation
 *********************************************************************/

NSAPI_PUBLIC ACLHandle_t * ACL_AclNew(NSErr_t *errp, char *tag);
NSAPI_PUBLIC void ACL_AclDestroy(NSErr_t *errp, ACLHandle_t *acl);
NSAPI_PUBLIC int ACL_ExprAppend(NSErr_t *errp, ACLHandle_t *acl, ACLExprHandle_t *expr);
NSAPI_PUBLIC const char *ACL_AclGetTag(ACLHandle_t *acl);

/*********************************************************************
 * ACL list manipulation
 *********************************************************************/

NSAPI_PUBLIC ACLListHandle_t * ACL_ListNew(NSErr_t *errp);
NSAPI_PUBLIC int ACL_ListConcat(NSErr_t *errp, ACLListHandle_t *acl_list1, ACLListHandle_t *acl_list2, int flags);
NSAPI_PUBLIC int ACL_ListAppend(NSErr_t *errp, ACLListHandle_t *acllist, ACLHandle_t *acl, int flags);
NSAPI_PUBLIC void ACL_ListDestroy(NSErr_t *errp, ACLListHandle_t *acllist);
NSAPI_PUBLIC ACLHandle_t * ACL_ListFind(NSErr_t *errp, ACLListHandle_t *acllist, char *aclname, int flags);
NSAPI_PUBLIC int ACL_ListAclDelete(NSErr_t *errp, ACLListHandle_t *acl_list, char *acl_name, int flags);
NSAPI_PUBLIC int ACL_ListGetNameList(NSErr_t *errp, ACLListHandle_t *acl_list, char ***name_list);
NSAPI_PUBLIC int ACL_NameListDestroy(NSErr_t *errp, char **name_list);
NSAPI_PUBLIC ACLHandle_t *ACL_ListGetFirst(ACLListHandle_t *acl_list,
                                           ACLListEnum_t *acl_enum);
NSAPI_PUBLIC ACLHandle_t *ACL_ListGetNext(ACLListHandle_t *acl_list,
                                           ACLListEnum_t *acl_enum);

/* Only used for asserts.  Probably shouldn't be publicly advertized */
extern int ACL_AssertAcllist( ACLListHandle_t *acllist );

/* Need to be ACL_LIB_INTERNAL */
NSAPI_PUBLIC int ACL_ListPostParseForAuth(NSErr_t *errp, ACLListHandle_t *acl_list);

/*********************************************************************
 * ACL evaluation 
 *********************************************************************/

NSAPI_PUBLIC int ACL_EvalTestRights(NSErr_t *errp, ACLEvalHandle_t *acleval, const char **rights, const char **map_generic, char **deny_type, char **deny_response, char **acl_tag, int *expr_num);
NSAPI_PUBLIC int ACL_CachableAclList(ACLListHandle_t *acllist);
NSAPI_PUBLIC ACLEvalHandle_t * ACL_EvalNew(NSErr_t *errp, pool_handle_t *pool);
NSAPI_PUBLIC void ACL_EvalDestroy(NSErr_t *errp, pool_handle_t *pool, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC void ACL_EvalDestroyNoDecrement(NSErr_t *errp, pool_handle_t *pool, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC int ACL_ListDecrement(NSErr_t *errp, ACLListHandle_t *acllist);
NSAPI_PUBLIC int ACL_EvalSetACL(NSErr_t *errp, ACLEvalHandle_t *acleval, ACLListHandle_t *acllist);
NSAPI_PUBLIC PList_t ACL_EvalGetSubject(NSErr_t *errp, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC int ACL_EvalSetSubject(NSErr_t *errp, ACLEvalHandle_t *acleval, PList_t subject);
NSAPI_PUBLIC PList_t ACL_EvalGetResource(NSErr_t *errp, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC int ACL_EvalSetResource(NSErr_t *errp, ACLEvalHandle_t *acleval, PList_t resource);

/*
 *	The following entities are only meant to be called by whole server
 *	products that include libaccess.  E.g. the HTTP server, the Directory
 *	server etc.  They should not be called by ACL callers, LASs etc.
 */

/*********************************************************************
 * ACL misc routines 
 *********************************************************************/

NSAPI_PUBLIC int ACL_Init(void);
NSAPI_PUBLIC int ACL_InitPostMagnus(void);
NSAPI_PUBLIC int ACL_LateInitPostMagnus(void);
NSAPI_PUBLIC void ACL_ListHashUpdate(ACLListHandle_t **acllistp);

NSAPI_PUBLIC int ACL_MethodNamesGet(NSErr_t *errp, char ***names, int *count);
NSAPI_PUBLIC int ACL_MethodNamesFree(NSErr_t *errp, char **names, int count);

NSAPI_PUBLIC int ACL_DatabaseNamesGet(NSErr_t *errp, char ***names, int *count);
NSAPI_PUBLIC int ACL_DatabaseNamesFree(NSErr_t *errp, char **names, int count);

NSAPI_PUBLIC int ACL_InitAttr2Index(void);
NSAPI_PUBLIC int ACL_Attr2Index(const char *attrname);
NSAPI_PUBLIC void ACL_Attr2IndexListDestroy();
NSAPI_PUBLIC void ACL_AttrGetterHashDestroy(void);
NSAPI_PUBLIC void ACL_Destroy(void);
NSAPI_PUBLIC void ACL_DestroyPools(void);

/*********************************************************************
 * ACL cache and flush utility 
 *********************************************************************/

NSAPI_PUBLIC int ACL_CacheCheck(char *uri, ACLListHandle_t **acllist_p);
NSAPI_PUBLIC int ACL_CacheCheckGet(char *uri, ACLListHandle_t **acllist_p);
NSAPI_PUBLIC void ACL_CacheEnter(char *uri, ACLListHandle_t **acllist_p);
NSAPI_PUBLIC void ACL_CacheEnterGet(char *uri, ACLListHandle_t **acllist_p);
NSAPI_PUBLIC int ACL_ListHashCheck(ACLListHandle_t **acllist_p);
NSAPI_PUBLIC void ACL_ListHashEnter(ACLListHandle_t **acllist_p);
NSAPI_PUBLIC int ACL_CacheFlush(void);
NSAPI_PUBLIC void ACL_Restart(void *clntData);
NSAPI_PUBLIC void ACL_CritEnter(void);
NSAPI_PUBLIC void ACL_CritExit(void);

/*********************************************************************
 * ACL CGI routines
 *********************************************************************/

NSAPI_PUBLIC void ACL_OutputSelector(char *name, char **item);


NSPR_END_EXTERN_C

#endif /* INTNSACL */

#endif

