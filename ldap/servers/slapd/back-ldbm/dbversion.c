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

 
#include "back-ldbm.h"

static void
mk_dbversion_fullpath(struct ldbminfo *li, const char *directory, char *filename)
{
    if (li)
    {
        if (is_fullpath((char *)directory))
        {
            PR_snprintf(filename, MAXPATHLEN*2, "%s/%s", directory, DBVERSION_FILENAME);
        }
        else
        {
            char *home_dir = dblayer_get_home_dir(li, NULL);
            /* if relpath, nsslapd-dbhome_directory should be set */
            PR_snprintf(filename, MAXPATHLEN*2,"%s/%s/%s", home_dir,directory,DBVERSION_FILENAME);
        }
    }
    else
    {
        PR_snprintf(filename, MAXPATHLEN*2, "%s/%s", directory, DBVERSION_FILENAME);
    }
}

/*
 *  Function: dbversion_write
 *
 *  Returns: returns 0 on success, -1 on failure
 *  
 *  Description: This function writes the DB version file.
 */
int
dbversion_write(struct ldbminfo *li, const char *directory,
                const char *dataversion, PRUint32 flags)
{
    char filename[ MAXPATHLEN*2 ];
    PRFileDesc *prfd;
    int rc = 0;

    if (!is_fullpath((char *)directory)) {
        rc = -1;
        return rc;
    }
        
    mk_dbversion_fullpath(li, directory, filename);
  
    /* Open the file */
    if (( prfd = PR_Open( filename, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
                          SLAPD_DEFAULT_FILE_MODE  )) == NULL )
    {
        LDAPDebug( LDAP_DEBUG_ANY, "Could not open file \"%s\" for writing "
                   SLAPI_COMPONENT_NAME_NSPR " %d (%s)\n",
                   filename, PR_GetError(), slapd_pr_strerror(PR_GetError()) );
        rc= -1;
    }
    else
    {
        /* Write the file */
        char buf[ LDBM_VERSION_MAXBUF ];
        char *ptr = NULL;
        size_t len = 0;
        /* Base DB Version */
        PR_snprintf(buf, sizeof(buf), "%s/%d.%d/%s",
                    BDB_IMPL, DB_VERSION_MAJOR, DB_VERSION_MINOR, BDB_BACKEND);
        len = strlen(buf);
        ptr = buf + len;
        if (idl_get_idl_new() && (flags & DBVERSION_NEWIDL)) {
            PR_snprintf(ptr, sizeof(buf) - len, "/%s", BDB_NEWIDL);
            len = strlen(buf);
            ptr = buf + len;
        }
        if (entryrdn_get_switch() && (flags & DBVERSION_RDNFORMAT)) {
            PR_snprintf(ptr, sizeof(buf) - len, "/%s-%s", 
                        BDB_RDNFORMAT, BDB_RDNFORMAT_VERSION);
            len = strlen(buf);
            ptr = buf + len;
        }
        if (flags & DBVERSION_DNFORMAT) {
            PR_snprintf(ptr, sizeof(buf) - len, "/%s-%s",
                        BDB_DNFORMAT, BDB_DNFORMAT_VERSION);
            len = strlen(buf);
            ptr = buf + len;
        }
        /* end in a newline */
        PL_strncpyz(ptr, "\n", sizeof(buf) - len);
        len = strlen(buf);
        if ( slapi_write_buffer( prfd, buf, len ) != len )
        {
            LDAPDebug( LDAP_DEBUG_ANY, "Could not write to file \"%s\"\n", filename, 0, 0 );
            rc= -1;
        }
        if(rc==0 && dataversion!=NULL)
        {
            sprintf( buf, "%s\n", dataversion );
            len = strlen( buf );
            if ( slapi_write_buffer( prfd, buf, len ) != len )
            {
                LDAPDebug( LDAP_DEBUG_ANY, "Could not write to file \"%s\"\n", filename, 0, 0 );
                rc= -1;
            }
        }
        (void)PR_Close( prfd );
    }
    return rc;
}

/*
 *  Function: dbversion_read
 *
 *  Returns: returns 0 on success, -1 on failure
 *  
 *  Description: This function reads the DB version file.
 */
int
dbversion_read(struct ldbminfo *li, const char *directory,
               char **ldbmversion, char **dataversion)
{
    char filename[ MAXPATHLEN*2 ];
    PRFileDesc *prfd;
    char * iter = NULL;
    PRFileInfo64 fileinfo;
    int rc;

    if (!is_fullpath((char *)directory)) {
        return ENOENT;
    }

    if (NULL == ldbmversion) {
        return EINVAL;
    }

    rc = PR_GetFileInfo64(directory, &fileinfo);
    if ((rc != PR_SUCCESS) || (fileinfo.type != PR_FILE_DIRECTORY)) {
        /* Directory does not exist or not a directory. */
        return ENOENT;
    }

    mk_dbversion_fullpath(li, directory, filename);
    
    /* Open the file */
    prfd = PR_Open(filename, PR_RDONLY, SLAPD_DEFAULT_FILE_MODE);
    if (prfd == NULL) {
        /* File missing... we are probably creating a new database. */
        return EACCES;
    } else {
        char buf[LDBM_VERSION_MAXBUF];
        PRInt32 nr = slapi_read_buffer(prfd, buf, (PRInt32)LDBM_VERSION_MAXBUF-1);
        if ( nr > 0 && nr != (PRInt32)LDBM_VERSION_MAXBUF-1 )
        {
            char *t;
            buf[nr] = '\0';
            t = ldap_utf8strtok_r(buf,"\n", &iter);
            if(NULL != t)
            {
                *ldbmversion = slapi_ch_strdup(t);
                t = ldap_utf8strtok_r(NULL,"\n", &iter);
                if(NULL != dataversion && t != NULL && t[0] != '\0')
                {
                    *dataversion = slapi_ch_strdup(t);
                }
            }
        }
        (void)PR_Close( prfd );
        return 0;
    }
}


/*
 *  Function: dbversion_exists
 *
 *  Returns: 1 for exists, 0 for not.
 *  
 *  Description: This function checks if the DB version file exists.
 */
int
dbversion_exists(struct ldbminfo *li, const char *directory)
{
    char filename[ MAXPATHLEN*2 ];
    PRFileDesc *prfd;

    mk_dbversion_fullpath(li, directory, filename);

    if (( prfd = PR_Open( filename, PR_RDONLY, SLAPD_DEFAULT_FILE_MODE )) ==
         NULL )
    {
        return 0;
    }
    (void)PR_Close( prfd );
    return 1;
}

