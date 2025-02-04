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

#if defined( ultrix ) || defined( nextstep )

#include <string.h>


char *strdup( char *s )
{
        char    *p;

        if ( (p = (char *) malloc( strlen( s ) + 1 )) == NULL )
                return( NULL );

        strcpy( p, s );

        return( p );
}

#else
typedef int SHUT_UP_DAMN_COMPILER;
#endif /* ultrix || nextstep */
