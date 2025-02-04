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
 *  start.c
 */

#include "back-ldbm.h"

static int initialized = 0;

int
ldbm_back_isinitialized()
{
    return initialized;
}

/*
 * Start the LDBM plugin, and all its instances.
 */
int
ldbm_back_start( Slapi_PBlock *pb )
{
  struct ldbminfo  *li;
  char *home_dir;
  int action;
  int retval; 

  LDAPDebug( LDAP_DEBUG_TRACE, "ldbm backend starting\n", 0, 0, 0 );

  slapi_pblock_get( pb, SLAPI_PLUGIN_PRIVATE, &li );

  /* parse the config file here */
  if (0 != ldbm_config_load_dse_info(li)) {
      LDAPDebug( LDAP_DEBUG_ANY, "start: Loading database configuration failed\n",
            0, 0, 0 );
      return SLAPI_FAIL_GENERAL;
  }

  /* register with the binder-based resource limit subsystem so that    */
  /* lookthroughlimit can be supported on a per-connection basis.        */
  if ( slapi_reslimit_register( SLAPI_RESLIMIT_TYPE_INT,
            LDBM_LOOKTHROUGHLIMIT_AT, &li->li_reslimit_lookthrough_handle )
            != SLAPI_RESLIMIT_STATUS_SUCCESS ) {
      LDAPDebug( LDAP_DEBUG_ANY, "start: Resource limit registration failed for lookthroughlimit\n",
            0, 0, 0 );
      return SLAPI_FAIL_GENERAL;
  }

  /* register with the binder-based resource limit subsystem so that    */
  /* allidslimit (aka idlistscanlimit) can be supported on a per-connection basis.        */
  if ( slapi_reslimit_register( SLAPI_RESLIMIT_TYPE_INT,
            LDBM_ALLIDSLIMIT_AT, &li->li_reslimit_allids_handle )
            != SLAPI_RESLIMIT_STATUS_SUCCESS ) {
      LDAPDebug( LDAP_DEBUG_ANY, "start: Resource limit registration failed for allidslimit\n",
            0, 0, 0 );
      return SLAPI_FAIL_GENERAL;
  }

  /* register with the binder-based resource limit subsystem so that    */
  /* pagedlookthroughlimit can be supported on a per-connection basis.        */
  if ( slapi_reslimit_register( SLAPI_RESLIMIT_TYPE_INT,
            LDBM_PAGEDLOOKTHROUGHLIMIT_AT, &li->li_reslimit_pagedlookthrough_handle )
            != SLAPI_RESLIMIT_STATUS_SUCCESS ) {
      LDAPDebug( LDAP_DEBUG_ANY, "start: Resource limit registration failed for pagedlookthroughlimit\n",
            0, 0, 0 );
      return SLAPI_FAIL_GENERAL;
  }

  /* register with the binder-based resource limit subsystem so that    */
  /* pagedallidslimit (aka idlistscanlimit) can be supported on a per-connection basis.        */
  if ( slapi_reslimit_register( SLAPI_RESLIMIT_TYPE_INT,
            LDBM_PAGEDALLIDSLIMIT_AT, &li->li_reslimit_pagedallids_handle )
            != SLAPI_RESLIMIT_STATUS_SUCCESS ) {
      LDAPDebug( LDAP_DEBUG_ANY, "start: Resource limit registration failed for pagedallidslimit\n",
            0, 0, 0 );
      return SLAPI_FAIL_GENERAL;
  }

  /* lookthrough limit for the rangesearch */
  if ( slapi_reslimit_register( SLAPI_RESLIMIT_TYPE_INT,
            LDBM_RANGELOOKTHROUGHLIMIT_AT, &li->li_reslimit_rangelookthrough_handle )
            != SLAPI_RESLIMIT_STATUS_SUCCESS ) {
      LDAPDebug( LDAP_DEBUG_ANY, "start: Resource limit registration failed for rangelookthroughlimit\n",
            0, 0, 0 );
      return SLAPI_FAIL_GENERAL;
  }

  /* If the db directory hasn't been set yet, we need to set it to 
   * the default. */
  if (NULL == li->li_directory || '\0' == li->li_directory[0]) {
      /* "get default" is a special string that tells the config
       * routines to figure out the default db directory by 
       * reading cn=config. */
      ldbm_config_internal_set(li, CONFIG_DIRECTORY, "get default");
  }

  /* sanity check the autosizing values,
     no value or sum of values larger than 100.
  */
  if ((li->li_cache_autosize > 100) ||
      (li->li_cache_autosize_split > 100) ||
      (li->li_import_cache_autosize > 100) ||
      ((li->li_cache_autosize > 0) && (li->li_import_cache_autosize > 0) &&
      (li->li_cache_autosize + li->li_import_cache_autosize > 100))) {
      LDAPDebug( LDAP_DEBUG_ANY, "cache autosizing: bad settings, "
        "value or sum of values can not larger than 100.\n", 0, 0, 0 );
  } else {
      size_t pagesize, pages, procpages, availpages;

      dblayer_sys_pages(&pagesize, &pages, &procpages, &availpages);
      if (pagesize) {
          char s[32];    /* big enough to hold %ld */
          unsigned long cache_size_to_configure = 0;
          int zone_pages, db_pages, entry_pages, import_pages;
          Object *inst_obj;
          ldbm_instance *inst;   
          PRUint64 cache_size;
          PRUint64 db_size;
          PRUint64 total_cache_size = 0;
          PRUint64 memsize = pages * pagesize;
          PRUint64 extra = 0; /* e.g., dncache size */

          for (inst_obj = objset_first_obj(li->li_instance_set); inst_obj;
               inst_obj = objset_next_obj(li->li_instance_set, inst_obj)) {
              inst = (ldbm_instance *)object_get_data(inst_obj);
              cache_size = (PRUint64)cache_get_max_size(&(inst->inst_cache));
              db_size = dblayer_get_id2entry_size(inst);
              if (cache_size < db_size) {
                  LDAPDebug(LDAP_DEBUG_ANY,
                            "WARNING: %s: entry cache size %" NSPRIu64 "B is "
                            "less than db size %" NSPRIu64 "B; "
                            "We recommend to increase the entry cache size "
                            "nsslapd-cachememsize.\n",
                            inst->inst_name, cache_size, db_size);
              } else {
                  LDAPDebug(LDAP_DEBUG_BACKLDBM,
                            "%s: entry cache size: %" NSPRIu64 "B; db size: %" NSPRIu64 "B\n",
                            inst->inst_name, cache_size, db_size);
              }
              total_cache_size += cache_size;
              /* estimated overhead: dncache size * 2 */
              extra += (PRUint64)cache_get_max_size(&(inst->inst_dncache)) * 2;
          }
          LDAPDebug(LDAP_DEBUG_BACKLDBM,
                    "Total entry cache size: %" NSPRIu64 "B; "
                    "dbcache size: %" NSPRIu64 "B; "
                    "available memory size: %" NSPRIu64 "B\n",
                    total_cache_size, (PRUint32)li->li_dbcachesize, memsize - extra);
          /* autosizing dbCache and entryCache */
          if (li->li_cache_autosize > 0) {
              zone_pages = (li->li_cache_autosize * pages) / 100;
              /* now split it according to user prefs */
              db_pages = (li->li_cache_autosize_split * zone_pages) / 100;
              /* fudge an extra instance into our calculations... */
              entry_pages = (zone_pages - db_pages) /
                      (objset_size(li->li_instance_set) + 1);
              LDAPDebug(LDAP_DEBUG_ANY, "cache autosizing. found %dk physical memory\n",
                pages*(pagesize/1024), 0, 0);
              LDAPDebug(LDAP_DEBUG_ANY, "cache autosizing: db cache: %dk, "
                "each entry cache (%d total): %dk\n",
                db_pages*(pagesize/1024), objset_size(li->li_instance_set),
                entry_pages*(pagesize/1024));
    
              /* libdb allocates 1.25x the amount we tell it to,
               * but only for values < 500Meg 
               * For the larger memory, the overhead is relatively small. */
              cache_size_to_configure = (unsigned long)(db_pages * pagesize);
              if (cache_size_to_configure < (500 * MEGABYTE)) {
                  cache_size_to_configure = (unsigned long)((db_pages * pagesize) / 1.25);
              }
              sprintf(s, "%lu", cache_size_to_configure);
              ldbm_config_internal_set(li, CONFIG_DBCACHESIZE, s);
              li->li_cache_autosize_ec = (unsigned long)entry_pages * pagesize;
    
              for (inst_obj = objset_first_obj(li->li_instance_set); inst_obj;
                  inst_obj = objset_next_obj(li->li_instance_set, inst_obj)) {
                  inst = (ldbm_instance *)object_get_data(inst_obj);
                  cache_set_max_entries(&(inst->inst_cache), -1);
                  cache_set_max_size(&(inst->inst_cache),
                                    li->li_cache_autosize_ec, CACHE_TYPE_ENTRY);
              }
          }    
          /* autosizing importCache */
          if (li->li_import_cache_autosize > 0) {
              /* For some reason, -1 means 50 ... */
              if (li->li_import_cache_autosize == -1) {
                    li->li_import_cache_autosize = 50;
              }
              import_pages = (li->li_import_cache_autosize * pages) / 100;
              LDAPDebug(LDAP_DEBUG_ANY, "cache autosizing: import cache: %dk \n",
                import_pages*(pagesize/1024), NULL, NULL);
    
              sprintf(s, "%lu", (unsigned long)(import_pages * pagesize));
              ldbm_config_internal_set(li, CONFIG_IMPORT_CACHESIZE, s);
          }
      }
  }

  retval = check_db_version(li, &action);
  if (0 != retval)
  {
      LDAPDebug( LDAP_DEBUG_ANY, "start: db version is not supported\n",
                 0, 0, 0);
      return SLAPI_FAIL_GENERAL;
  }

  if (action &
      (DBVERSION_UPGRADE_3_4|DBVERSION_UPGRADE_4_4|DBVERSION_UPGRADE_4_5))
  {
      retval = dblayer_start(li,DBLAYER_CLEAN_RECOVER_MODE);
  }
  else
  {
      retval = dblayer_start(li,DBLAYER_NORMAL_MODE);
  }
  if (0 != retval) {
      char *msg;
      LDAPDebug( LDAP_DEBUG_ANY, "start: Failed to init database, err=%d %s\n",
         retval, (msg = dblayer_strerror( retval )) ? msg : "", 0 );
      if (LDBM_OS_ERR_IS_DISKFULL(retval)) return return_on_disk_full(li);
      else return SLAPI_FAIL_GENERAL;
  }

  /* Walk down the instance list, starting all the instances. */
  retval = ldbm_instance_startall(li);
  if (0 != retval) {
      char *msg;
      LDAPDebug( LDAP_DEBUG_ANY, "start: Failed to start databases, err=%d %s\n",
         retval, (msg = dblayer_strerror( retval )) ? msg : "", 0 );
      if (LDBM_OS_ERR_IS_DISKFULL(retval)) return return_on_disk_full(li);
      else {
        if ((li->li_cache_autosize > 0) && (li->li_cache_autosize <= 100)) {
          LDAPDebug( LDAP_DEBUG_ANY, "Failed to allocate %d byte dbcache.  "
                   "Please reduce the value of %s and restart the server.\n",
                   li->li_dbcachesize, CONFIG_CACHE_AUTOSIZE, 0);
        }
        return SLAPI_FAIL_GENERAL;
      }
  }

  /* write DBVERSION file if one does not exist */
  home_dir = dblayer_get_home_dir(li, NULL);
  if (!dbversion_exists(li, home_dir))
  {
      dbversion_write (li, home_dir, NULL, DBVERSION_ALL);
  }


  /* this function is called every time new db is initialized   */
  /* currently it is called the 2nd time  when changelog db is  */
  /* dynamically created. Code below should only be called once */
  if (!initialized)
  {
    ldbm_compute_init();

    initialized = 1;
  }

  /* initialize the USN counter */
  ldbm_usn_init(li);

  LDAPDebug( LDAP_DEBUG_TRACE, "ldbm backend done starting\n", 0, 0, 0 );

  return( 0 );

}
