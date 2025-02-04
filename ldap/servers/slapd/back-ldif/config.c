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
 *  File: config.c
 *
 *  Functions:
 * 
 *     ldif_back_config() - Reads in and stores ldif file for ldif backend
 *     ldif_read_one_record() - Reads in one record from ldif file as a string 
 *
 */

#include "back-ldif.h"
#define LDAPMOD_MAXLINE              4096
#define safe_realloc( ptr, size )	( ptr == NULL ? malloc( size ) : \
					 realloc( ptr, size ))
static char *ldif_read_one_record();


/*
 *  Function: ldif_back_config
 *  
 *  Returns: 0 if success, -1 if not
 *  
 *  Description: Reads the data in the ldif file specified in ldif.conf
 *               stores it in db
 */
int
ldif_back_config( Slapi_PBlock *pb )
{
  LDIF  *db;                  /*The ldif file will be read into this structure*/
  char  *fname;               /*Config file name*/
  int   lineno, argc;         /*Config file stuff*/
  char  **argv;               /*More config file stuff*/
  FILE  *fp;                  /*Pointer to ldif file*/
  char  *buf;                 /*Tmp storage for ldif entries*/
  int   first;                /*Boolean to determine if db is empty*/
  ldif_Entry *cur;            /*For db manipulation*/
  ldif_Entry *new;            /*For db manipulation*/
  Slapi_Entry *tmp;                 /*Used for initialization purposes*/


  LDAPDebug( LDAP_DEBUG_TRACE, "=> ldif_back_config\n", 0, 0, 0 );
  
  /* 
   * Get the private_info structure you created in ldif_back_init(). 
   * Also get the config file name, current line number, and arguments
   * from the current line, broken out into an argv.
   */
  if (slapi_pblock_get( pb, SLAPI_PLUGIN_PRIVATE, &db ) < 0 ||
      slapi_pblock_get( pb, SLAPI_CONFIG_FILENAME, &fname ) < 0 ||
      slapi_pblock_get( pb, SLAPI_CONFIG_LINENO, &lineno ) < 0 ||
      slapi_pblock_get( pb, SLAPI_CONFIG_ARGC, &argc ) < 0 ||
      slapi_pblock_get( pb, SLAPI_CONFIG_ARGV, &argv ) < 0 ){
    LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: unable to get data from front end\n", 0, 0, 0);
    return(-1);
  }

  
  /*
   * Process the config info. For example, if the config file
   * contains a line like this:
   *
   * 	file	/path/to/ldif/file
   *
   * then argv[0] would be "file", and argv[1] would be the file
   * name. 
   */
  
  /*Check for the correct number of arguments*/
  if (argc != 2){
    LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: Unable to configure; invalid ldif input file specification line format (file: %s, line: %d)\n", 
	    fname, lineno, 0); 
    return(-1);
  }
  
  /*Check for the right format*/
  if (strcmp(argv[0], "file") != 0){
    LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: unable to configure; invalid parameter \"%s\" in config file (file: %s, line: %d)\n", 
	    argv[0], fname, lineno ); 
    return(-1);
  }
  
  /*Now we fopen the file and grab up the contents*/
  fp = fopen (argv[1], "r");
  if (fp == NULL){
    LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: unable to read ldif file %s\n", argv[1], 0, 0);
    fp = fopen (argv[1], "w");
    if(fp == NULL){
      LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: unable to create ldif file %s\n", argv[1], 0, 0);
      return -1;
    }
  }
    
  first = 1;

  /* Lock the database first, just to be safe*/
  PR_Lock( db->ldif_lock );

  /*Save the filename, for modifications to the file later*/
  if ((db->ldif_file = strdup(argv[1])) == NULL){
    LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: out of memory\n", 0, 0, 0);
    PR_Unlock( db->ldif_lock );
    fclose(fp);
    return(-1);
  }
  
  /* 
   * Loop through the entries in the file, and add them to the end
   * of the linked list
   */
  while ((buf = ldif_read_one_record(fp)) != NULL){

    /*Create a new element for the linked list of entries*/
    tmp = (Slapi_Entry *) slapi_str2entry(buf,
				SLAPI_STR2ENTRY_NOT_WELL_FORMED_LDIF);
    new = (ldif_Entry *) ldifentry_init(tmp);
    if (new == NULL){
      LDAPDebug(LDAP_DEBUG_ANY, "Ldif Backend: unable to read in ldif file; out of memory\n",0 ,0 ,0 );
      PR_Unlock( db->ldif_lock );
      fclose(fp);
      return(-1);
    }
    
    /*
     * If this is the first entry we are adding,
     * we have to make it the first element in the list
     */
    if (first){
      db->ldif_entries = new;
      first = 0;
    } else{
      cur->next = new;
    }
    
    /*Reset the pointer to the last element in the list*/
    cur = new;

    /*Increment the number of entries*/
    db->ldif_n++;

    /*Free the buffer*/
    free ((void *) buf );

  }
    
  /*By now, the database should be read in*/
  PR_Unlock( db->ldif_lock );
  fclose(fp);

  LDAPDebug( LDAP_DEBUG_TRACE, "<= ldif_back_config\n", 0, 0, 0 );
  return( 0 );
}



/*
 *  Function: ldif_read_one_record
 *  
 *  Returns: a long string representing an ldif record
 *  
 *  Description: Returns a huge string comprised of 1 ldif record
 *               read from fp.
 *
 */
static char *
ldif_read_one_record( FILE *fp )
{
  int   len, gotnothing;
  char  *buff, line[ LDAPMOD_MAXLINE ];
  int	lcur, lmax;
  
  lcur = lmax = 0;
  buff = NULL;
  gotnothing = 1;
  
  while ( fgets( line, sizeof(line), fp ) != NULL ) {
    if ( (len = strlen( line )) < 2 ) {
      if ( gotnothing ) {
	continue;
      } else {
	break;
      }
    }
    gotnothing = 0;
    if ( lcur + len + 1 > lmax ) {
      lmax = LDAPMOD_MAXLINE
	* (( lcur + len + 1 ) / LDAPMOD_MAXLINE + 1 );
      if (( buff = (char *)safe_realloc( buff, lmax )) == NULL ) {
	perror( "safe_realloc" );
	exit( LDAP_NO_MEMORY );
      }
    }
    strcpy( buff + lcur, line );
    lcur += len;
  }
  
  return( buff );
}

