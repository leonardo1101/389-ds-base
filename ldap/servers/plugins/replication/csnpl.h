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

/* csnpl.h - interface for csn pending list */

#ifndef CSNPL_H
#define CSNPL_H

#include "slapi-private.h"

typedef struct csnpl CSNPL;

CSNPL* csnplNew ();
void csnplFree (CSNPL **csnpl);
int csnplInsert (CSNPL *csnpl, const CSN *csn);
int csnplRemove (CSNPL *csnpl, const CSN *csn);
CSN* csnplGetMinCSN (CSNPL *csnpl, PRBool *committed);
int csnplCommit (CSNPL *csnpl, const CSN *csn);
CSN *csnplRollUp(CSNPL *csnpl, CSN ** first);
void csnplDumpContent(CSNPL *csnpl, const char *caller); 
#endif
