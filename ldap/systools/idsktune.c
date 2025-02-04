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


/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *    Don't forget to update build_date when the patch sets are updated. 
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
static char *build_date = "23-FEBRUARY-2012";

#if defined(linux) || defined(__linux) || defined(__linux__)
#define IDDS_LINUX_INCLUDE 1
#define IDDS_LINUX_SYSCTL 1
#endif

#if defined(__sun) || defined(__sun__)|| defined(__hpux)
#define IDDS_SYSV_INCLUDE 1
#endif

#include <sys/types.h>

#if !defined(__VMS)
#include <sys/resource.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#if !defined(__VMS) && !defined(IDDS_LINUX_INCLUDE)
#if defined(__hpux) && defined(f_type)
#undef f_type
#endif
#include <sys/statvfs.h>
#define IDDS_HAVE_STATVFS
#endif

#if defined(IDDS_SYSV_INCLUDE) || defined(IDDS_BSD_INCLUDE)
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/param.h>
#endif

#if defined(__sun) || defined(__sun__)
/* not available on HP-UX or AIX */
#include <sys/systeminfo.h>
#endif

#if defined(__sun)
#include <stdio.h>
#include <fcntl.h>
#include <sys/mnttab.h>
#define IDDS_MNTENT mnttab
#define IDDS_MNTENT_DIRNAME mnt_mountp
#define IDDS_MNTENT_OPTIONS mnt_mntopts
#define IDDS_MNTENT_MNTTAB "/etc/mnttab"
#endif

#if defined(IDDS_LINUX_INCLUDE)
#include <sys/vfs.h>
#include <sys/utsname.h>
#include <linux/kernel.h>
#include <sys/time.h>
#include <sys/param.h>
#include <mntent.h>
#include <sys/sysinfo.h>

#define IDDS_MNTENT mntent
#define IDDS_MNTENT_DIRNAME mnt_dir
#define IDDS_MNTENT_OPTIONS mnt_opts
#define IDDS_MNTENT_MNTTAB "/etc/mtab"
#endif

#if defined(__hpux)
#include <sys/pstat.h>
#include <mntent.h>

#define IDDS_MNTENT mntent
#define IDDS_MNTENT_DIRNAME mnt_dir
#define IDDS_MNTENT_OPTIONS mnt_opts
#define IDDS_MNTENT_MNTTAB "/etc/mnttab"
#endif

#if defined(__VMS)
#error "This program is not available for VMS"
#endif

#if defined(__osf__)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <machine/hal_sysinfo.h>
#include <machine/cpuconf.h>
#include <fstab.h>

#define IDDS_MNTENT fstab
#define IDDS_MNTENT_DIRNAME fs_file
#define IDDS_MNTENT_OPTIONS fs_mntops
#endif


#include "pio.h"

int flag_html = 0;
int flag_debug = 0;
int flag_quick = 0;
int flag_os_bad = 0;
int flag_solaris_9 = 0;
int flag_solaris_8 = 0;
int flag_solaris_7 = 0;
int flag_solaris_26 = 0;
int flag_solaris_251 = 0;
int flag_tru64_40b = 0;
int flag_tru64_tuning_needed = 0;
int solaris_version = 0;
int flag_intel = 0;
int flag_arch_bad = 0;
int phys_mb = 0;
int swap_mb = 0;
int flag_mproc = 0;
int tcp_max_listen = 0;
int flag_nonroot = 0;
int flag_carrier = 0;
int flag_warnings = 0;

int conn_thread = 1;
int maxconn = 0;
int client = 0;

int mem_min = 256;
int mem_rec = 1024;

/* Define name variations on different platforms */

#if defined(__sun)
#define NAME_NDD_CFG_FILE			"/etc/init.d/inetinit"
#define NAME_TCP_TIME_WAIT_INTERVAL	"tcp_time_wait_interval"
#define NAME_TCP_CONN_REQ_MAX_Q		"tcp_conn_req_max_q"
#define NAME_TCP_KEEPALIVE_INTERVAL	"tcp_keepalive_interval"
#define NAME_TCP_SMALLEST_ANON_PORT	"tcp_smallest_anon_port"
#endif

#if defined(__hpux)
#define NAME_NDD_CFG_FILE			"/etc/rc.config.d/nddconf"
#define NAME_TCP_TIME_WAIT_INTERVAL	"tcp_time_wait_interval"
#define NAME_TCP_CONN_REQ_MAX_Q		"tcp_conn_request_max"
#define NAME_TCP_KEEPALIVE_INTERVAL	"tcp_keepalive_interval"
#define NAME_TCP_SMALLEST_ANON_PORT	"tcp_smallest_anon_port"
#endif

#if defined(IDDS_LINUX_SYSCTL)
#define NAME_TCP_KEEPALIVE_INTERVAL	"net.ipv4.tcp_keepalive_time"
#endif

#if defined(__sun) || defined(__hpux) || defined(IDDS_LINUX_SYSCTL)

long ndd_tcp_conn_req_max_q = 0; 
long ndd_tcp_conn_req_max_q0 = 0; 
long ndd_tcp_rexmit_interval_initial = 0; 
long ndd_tcp_slow_start_initial = 0;
long ndd_tcp_keepalive_interval = 0; 
long ndd_tcp_time_wait_interval = 0; 
long ndd_tcp_smallest_anon_port = 0; 
long ndd_tcp_deferred_ack_interval = 0;
long ndd_tcp_ip_abort_cinterval = 0;
long ndd_tcp_ip_abort_interval = 0;
long ndd_tcp_strong_iss = 0;
int hpux_ndd_change_needed = 0;


#endif

char install_dir[MAXPATHLEN];
int avail_root = 0;
int avail_opt = 0;
int max_core = 0;

#if defined(__sun)
#define SUN_NETWORK_DEVICE "/dev/hme"

char solaris_release_string[128];

struct iii_pinfo {
  int pnum;
  int pver;
  int preq;
  int sol;
  int intel;
  int seen;
  char *pdesc;
} iii_patches[] = {

#include "sol_patches.c"
/* sol_patches.c is a generated file
   if you need to include patches not automatically
   generated, please list them individually here
*/

  {0,0,0,0,0}
};

#endif

#if defined(__hpux)
struct pst_dynamic pst_dyn;
struct pst_static pst_stt;
struct pst_vminfo pst_vmm;

struct iii_pinfo_hp {
  char *qpk_name;
  char *qpk_version;
  char *qpk_desc;
  int  qpk_yr;
  int  qpk_mo;
  int  seen;
} iii_qpk[] = {

#include "hp_patches.c"
/* hp_patches.c is a generated file
   if you need to include patches not automatically
   generated, please list them individually here
*/
  {NULL, NULL, NULL, 0, 0, 0}
};

static void hp_check_qpk(void);
#endif

#if defined(__sun)

static void ids_get_platform_solaris(char *buf)
{
    struct utsname u;
    char sbuf[128];
    FILE *fp;
  
#if defined(sparc) || defined(__sparc)
    int is_u = 0;
    
    sbuf[0] = '\0';
    sysinfo(SI_MACHINE,sbuf,128);
    
    if (strcmp(sbuf,"sun4u") == 0) {
      is_u = 1;
    }
    
    sbuf[0] = '\0';
    sysinfo(SI_PLATFORM,sbuf,128);
    
    sprintf(buf,"%ssparc%s-%s-solaris",
	    is_u ? "u" : "",
	    sizeof(long) == 4 ? "" : "v9",
	    sbuf);
#else
#if defined(i386) || defined(__i386)
    sprintf(buf,"i386-unknown-solaris");
#else
    sprintf(buf,"unknown-unknown-solaris");
#endif /* not i386 */
#endif /* not sparc */

    uname(&u);
    if (isascii(u.release[0]) && isdigit(u.release[0])) strcat(buf,u.release);

    fp = fopen("/etc/release","r");

    if (fp != NULL) {
      char *rp;

      sbuf[0] = '\0';
      fgets(sbuf,128,fp);
      fclose(fp);
      rp = strstr(sbuf,"Solaris");
      if (rp) {
	rp += 8;
	while(*rp != 's' && *rp != '\0') rp++;
	if (*rp == 's') {
	  strcpy(solaris_release_string,rp);
	  rp = strchr(solaris_release_string,' ');
	  if (rp) *rp = '\0';
	  strcat(buf,"_");
	  strcat(buf,solaris_release_string);
	}
      }
    } else {
      if (flag_debug) printf("DEBUG  : No /etc/release file\n");
    }
}

int sun_check_kern_arch(char *a,char *b)
{
  if (strcmp(b,"i86pc") == 0) {
    flag_intel = 1;
    if (flag_debug) printf("DEBUG  : Kernel architecture: i86pc\n");
  } else if (strcmp(b,"sun4u") == 0) {
    if (flag_debug) printf("DEBUG  : Kernel architecture: sun4u\n");
  } else {
    if (flag_html) printf("<P>\n");
    printf("%s: The kernel architecture is %s.  " VENDOR " products are optimized\nfor the UltraSPARC architecture and will exhibit poorer performance on earlier\nmachines.\n\n","WARNING",b);
    if (flag_html) printf("</P>\n");
    flag_arch_bad = 1;
    flag_warnings++;
  }
  return 0;
}

/* This check is now obsolete.  See solaris_check_mu() */
int sun_check_release(char *a,char *b)
{
  if (flag_html) printf("<P>\n");
  if (strcmp(b,"9") == 0 ||
      strcmp(b,"5.9") == 0) {      
    if (flag_debug) printf("DEBUG  : Release 5.9\n");
    solaris_version = 29;
    flag_solaris_9 = 1;
  } else if (strcmp(b,"8") == 0 || 
      strcmp(b,"5.8") == 0) {
    if (flag_debug) printf("DEBUG  : Release 5.8\n");
    solaris_version = 28;
    flag_solaris_8 = 1;
  } else if (strcmp(b,"7") == 0 || 
      strcmp(b,"5.7") == 0) {
    if (flag_debug) printf("DEBUG  : Release 5.7\n");
    solaris_version = 27;
    flag_solaris_7 = 1;
  } else if (strcmp(b,"5.6") == 0) {
    flag_solaris_26 = 1;
    solaris_version = 26;
    if (flag_debug) printf("DEBUG  : Release 5.6\n");
  } else if (strcmp(b,"5.5.1") == 0) {
    if (client == 0) {
      printf("%s: Solaris versions prior to 2.6 are not suitable for running Internet\nservices; upgrading to 2.6 or later is required.\n\n",
	     "ERROR  ");
      flag_os_bad = 1;    
    }
    flag_solaris_251 = 1;
    solaris_version = 251;
  } else if (strcmp(b,"5.5") == 0) {
    printf("%s: Solaris versions prior to 2.6 are not suitable for running Internet\nservices; upgrading to 2.6 or later is required.\n\n",
	   "ERROR  ");
    flag_os_bad = 1;    
    solaris_version = 25;
  } else if (strcmp(b,"5.4") == 0) {
    printf("%s: Solaris versions prior to 2.6 are not suitable for running Internet\nservices; upgrading to 2.6 is required.\n\n",
	   "ERROR  ");
    flag_os_bad = 1;
    solaris_version = 24;
  } else if (strcmp(b,"5.3") == 0) {
    printf("%s: Solaris 2.3 is not supported.\n\n",
	   "ERROR  ");
    flag_os_bad = 1;    
  } else {
    printf("%s: Solaris version %s, as reported by showrev -a, is unrecognized.\n\n",
	   "NOTICE ",b);
    flag_os_bad = 1;    
  }
  if (flag_html) printf("</P>\n");

  return 0;
}

/* This check is now obsolete.  See solaris_check_mu() instead. */
int sun_check_kern_ver(char *a,char *b)
{
  char *rp,*pp;
  int pw = 0;

  if (flag_html) printf("<P>\n");  
  rp = strrchr(b,' ');
  if (rp == NULL || rp == b) {
    printf("NOTICE : Kernel version: \"%s\" not recognized.\n\n",b);
    if (flag_html) printf("</P>\n");
    return 0;
  }
  *rp = '\0';
  rp++;
  pp = strrchr(b,' ');
  if (pp == 0) {
    printf("NOTICE : Kernel version: \"%s\" not recognized.\n\n",b);
    if (flag_html) printf("</P>\n");
    return 0;
  }
  pp++;
  
  if (strcmp(rp,"1997") == 0 ||
      strcmp(rp,"1996") == 0) {
    printf("%s: This kernel was built in %s %s. Kernel builds prior to\n"
	   "mid-1998 are lacking many tuning fixes, uprading is strongly recommended.\n\n","ERROR  ",pp,rp);
    pw = 1;
    flag_os_bad = 1;
  } 

  if (flag_solaris_26 && strcmp(rp,"1998") == 0) {
    if (strncmp(pp,"Jan",3) == 0 ||
	strncmp(pp,"Feb",3) == 0 ||
	strncmp(pp,"Mar",3) == 0 ||
	strncmp(pp,"Apr",3) == 0) {
      printf("%s: This kernel was built in %s %s. Kernel builds prior to\n"
	     "mid-1998 are lacking many tuning fixes, uprading is strongly recommended.\n\n","WARNING",pp,rp);
      pw = 1;
      flag_os_bad = 1;
      flag_warnings++;
    }
  }

  if (flag_debug && pw == 0) {
    printf("DEBUG  : Kernel build date %s %s\n",pp,rp);
  }
  if (flag_html) printf("</P>\n");
  
  return 0;
}

int sun_check_patch(char *a,char *b)
{
  char *rp;
  int pid = 0;
  int pver = 0;
  int i;

  rp = strchr(b,'O');
  if (rp == NULL) {
    printf("%s: Cannot parse patch line %s\n","NOTICE ",b);
    return 0;
  }
  *rp = '\0';
  rp = strchr(b,'-');
  if (b == NULL) {
    printf("%s: Cannot parse patch line %s\n","NOTICE ",b);
  }
  *rp = '\0';
  rp++;
  pid = atoi(b);
  pver = atoi(rp);
  
  for (i = 0; iii_patches[i].pnum != 0; i++) {
    if (iii_patches[i].pnum == pid) {
      if (iii_patches[i].seen < pver) {
	iii_patches[i].seen = pver;
      }
      break;
    }
  }
  
  return 0;
}


int sun_check_etcsystem(void)
{
  FILE *fp;
  char buf[8192];
  int pp_set = -1;
  int tchs = 0;

  fp = fopen("/etc/system","r");

  if (fp == NULL) {
    perror("/etc/system");
    return -1;
  }
  while(fgets(buf,8192,fp) != NULL) {
    char *rp;
    rp = strchr(buf,'\n');
    if (rp) {
      *rp = '\0';
    }
    if (buf[0] == '*') continue;  /* comment */
    if (strncmp(buf,"set",3) != 0) continue;
    if (flag_debug) printf("DEBUG  : saw %s in /etc/system\n",buf);
    if (strstr(buf,"priority_paging") != NULL) {
      rp = strchr(buf,'=');
      if (rp == NULL) continue;
      rp++;
      while(isspace(*rp)) rp++;
      if (*rp == '1') {
	pp_set = 1;
      } else {
	pp_set = 0;
      }
    } else if (strstr(buf,"tcp:tcp_conn_hash_size") != NULL) {
      rp = strchr(buf,'=');
      if (rp == NULL) continue;
      rp++;
      while(isspace(*rp)) rp++;
      tchs = atoi(rp);
    }
  }
  fclose(fp);

  /* ias tuning recommends setting tcp_conn_hash_size to 8192 XXX */

  if (pp_set == 1) {
    if (flag_debug) printf("DEBUG  : saw priority_paging=1 in /etc/system\n");
  } else if (flag_solaris_7) {
    if (pp_set == 0) {
      printf("NOTICE : priority_paging is set to 0 in /etc/system.\n\n");
    } else {
      printf("WARNING: priority_paging is not set to 1 in /etc/system.  See Sun FAQ 2833.\n\n");
      flag_warnings++;
    }
  }


}

void sun_check_mu(void)
{
  if (solaris_release_string[0] == '\0') {
    if (flag_debug) printf("DEBUG  : No /etc/release line parsed\n");
    return;
  }
  
  switch(solaris_version) {
  case 28:
    if (strcmp(solaris_release_string,"s28_27b") == 0) {
      printf("ERROR  : This machine appears to be running Solaris 8 Beta.\n\n");
    } else if (strcmp(solaris_release_string,"s28_32d") == 0) {
      printf("ERROR  : This machine appears to be running Solaris 8 Beta Refresh.\n\n");
    } else if (strcmp(solaris_release_string,"s28_38shwp2") == 0) {
      printf("NOTICE : This machine appears to be running Solaris 8 FCS.  Solaris 8 Update\n4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28s_u1wos_08") == 0) {
      printf("NOTICE : This machine appears to be running Solaris 8 Update 1.  Solaris 8\nUpdate 4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28_38shwp2") == 0) {
      printf("NOTICE : This machine appears to be running Solaris 8 Maint Update 1.\nSolaris 8 Update 4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28s_u2wos_11b") == 0) {
      printf("NOTICE : This machine appears to be running Solaris 8 Update 2.  Solaris 8\nUpdate 4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28_38shwp2") == 0) {
      printf("NOTICE : This machine appears to be running Solaris 8 Maint Update 2.\nSolaris 8 Update 4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28s_u3wos_08") == 0) {
      printf("NOTICE : This machine appears to be running Solaris 8 Update 3.  Solaris 8\nUpdate 4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28s_u4wos_03") == 0) {
      printf("ERROR  : This machine appears to be running Solaris 8 Update 4 BETA. Solaris 8\nUpdate 4 has been released subsequent to this.\n\n");
    } else if (strcmp(solaris_release_string,"s28s_u4wos_08") == 0) {
      if (flag_debug) printf("DEBUG  : Solaris 8 Update 4 (4/01)\n\n");
    } else if (strcmp(solaris_release_string,"s28s_u5wos_08") == 0) {
      if (flag_debug) printf("DEBUG  : Solaris 8 Update 5\n\n");
    } else {
      if (flag_debug) printf("DEBUG  : Solaris 8 Unrecognized\n"); 
    }
    break;
  case 29:
    if (flag_debug) printf("DEBUG  : Solaris 9 Unrecognized\n"); 
    break;
  default:
    if (flag_debug) printf("DEBUG  : Solaris Unrecognized\n"); 
    /* probably pretty old */
    break;
  }

}

#endif

int check_memsize(char *a,char *b)
{
  char *rp;
  int mult = 1;

  rp = strchr(b,' ');
  
  if (rp == NULL) {
    printf("%s: Cannot parse Memory size: line %s\n\n","NOTICE ",b);
    return 0;
  }
  if (!isdigit(*b)) {
    printf("%s: Cannot parse Memory size: line %s\n\n","NOTICE ",b);
    return 0;
  }
  
  *rp = '\0';
  rp++;
  if (strcmp(rp,"Megabytes") == 0) {
  } else if (strcmp(rp,"Gigabytes") == 0) {
    mult = 1024;
  } else {
    printf("%s: Cannot parse Memory size: line %s %s\n\n","NOTICE ",b,rp);
    return 0;
  }
  phys_mb = atoi(b) * mult;
  

  return 0;
}

int check_swapsize(char *a,char *b)
{
  char *rp,*kp;
  int used, avail;

  rp = strchr(b,'=');

  if (rp == NULL) {
    printf("NOTICE : Cannot parse swap -s output: %s\n", b);
    return 0;
  }
  rp++;
  if (isspace(*rp)) rp++;
  if (!(isdigit(*rp))) {
    printf("NOTICE : Cannot parse swap -s output: %s\n", b);
    return 0;
  }
  kp = strchr(rp,'k');
  if (kp == NULL) {
    printf("NOTICE : Cannot parse swap -s output: %s\n", b);
    return 0;
  }

  *kp = '\0';
  used = atoi(rp);
  *kp = 'k';

  rp = strchr(b,',');

  if (rp == NULL) {
    printf("NOTICE : Cannot parse swap -s output: %s\n", b);
    return 0;
  }
  rp++;
  if (isspace(*rp)) rp++;
  if (!(isdigit(*rp))) {
    printf("NOTICE : Cannot parse swap -s output: %s\n", b);
    return 0;
  }
  
  kp = strchr(rp,'k');
  if (kp == NULL) {
    printf("NOTICE : Cannot parse swap -s output: %s\n", b);
    return 0;
  }
  *kp = '\0';
  avail = atoi(rp);

  if (flag_debug) {
    printf("DEBUG  : swap used %dK avail %dK\n", used, avail);
  }

  swap_mb = (used+avail)/1024-(phys_mb*7/8);
  

  return 0;
}

#if defined(__sun)

int check_kthread(char *a,char *b)
{
  flag_mproc = 1;
  return 0;
}

int check_tcpmaxlisten(char *a,char *b)
{
  if (flag_os_bad == 1) return 0;

  if (client) return 0;

  if (flag_html) printf("<P>\n");

  if (isdigit(*b)) {
    tcp_max_listen = atoi(b);
    if (tcp_max_listen <= 1024) {
      printf("%s: The kernel has been configured with a maximum of 1024 for the listen\nbacklog queue size.  This will prevent it from being raised with ndd.\n","ERROR  ");

      if (flag_mproc == 0) {
	printf("The following line should be added to the file /etc/init.d/inetinit:\n");
	if (flag_html) printf("</P><PRE>\n");
	printf("echo \"tcp_param_arr+14/W 0t65536\" | adb -kw /dev/ksyms /dev/mem\n");
	printf("\n");
	if (flag_html) printf("</PRE><P>\n");
      } else {
	printf("As this is a multiprocessor, contact your Solaris support representative for\ninformation on changing the parameter at tcp_param_arr+14, as documented in\nSun Security Bulletin #00136 section B.1.\n\n");
      }

    } else if (tcp_max_listen < 65536) {
      
      printf("%s: The kernel has a configured limit of %d for the maximum listen\nbacklog queue size.  Setting a value larger than this with ndd will not have\nan effect.\n","WARNING",tcp_max_listen);
      flag_warnings++;
      
      if (flag_mproc == 0) {
	printf("The value can be raised by adding the following line to the file\n/etc/init.d/inetinit:\n");

	if (flag_html) printf("</P><PRE>\n");	
	printf("echo \"tcp_param_arr+14/W 0t65536\" | adb -kw /dev/ksyms /dev/mem\n");
	if (flag_html) printf("</PRE><P>\n");
	printf("\n\n");
      }

    } else {
      if (flag_debug) printf("DEBUG  : tcp_param_arr+0x14: %d\n",tcp_max_listen);
    }
    
  } else if (strcmp(b,"-1") == 0) {
    /* OK I guess */
    tcp_max_listen = 65535;
    
  } else {
    printf("NOTICE : tcp_param_arr+0x14: %s cannot be parsed\n\n", b);
  }
  
  if (flag_html) printf("</P>\n");

  return 0;
}

struct iii_pio_parsetab ptb_showrev[] = {
  {"Release",sun_check_release},
  {"Kernel architecture",sun_check_kern_arch},
  {"Kernel version",sun_check_kern_ver},
  {"Patch",sun_check_patch}
};

struct iii_pio_parsetab ptb_adb[] = {
  {"   kernel thread at:",check_kthread},
  {"tcp_param_arr+0x14",check_tcpmaxlisten}
};

#endif

struct iii_pio_parsetab ptb_prtconf[] = {
  {"Memory size",check_memsize}
};

struct iii_pio_parsetab ptb_swap[] = {
  {"total",check_swapsize}
};

#if defined(IDDS_LINUX_INCLUDE)
static void
linux_check_release(void)
{
  FILE *fp;
  char osl[128];
  char *cmd = strdup("/bin/uname -r");
  int major = 0;
  int minor = 0;
  int revision = 0;

  if (cmd == NULL) {
    printf("ERROR: Unable to allocate memory\n");
    goto done;
  }

  if (flag_html) printf("<P>\n");
  if (flag_debug) printf("DEBUG  : %s\n",cmd);
  fp = popen(cmd,"r");

  if (fp == NULL) {
    perror("popen");
    goto done;
  }

  if (fgets(osl,128,fp) == NULL) {
	printf("WARNING: Cannot determine the kernel number.\n");
	pclose(fp);
	flag_warnings++;
	goto done;
  }
  pclose(fp);

  if (flag_debug) {
	printf("DEBUG  : %s\n",osl);
  }

  major = atoi(strtok(osl, "."));
  minor = atoi(strtok(NULL, "."));
  revision = atoi(strtok(NULL, "-"));

  if (major < 2) {
	printf("ERROR: We support kernel version 2.4.7 and higher.\n\n");
	flag_os_bad = 1;
	goto done;
  } else if (major == 2) {
	if (minor < 4) {
		printf("ERROR: We support kernel version 2.4.7 and higher.\n\n");
		flag_os_bad = 1;
		goto done;
	} else if (minor == 4) {
		if (revision < 7) {
			printf("ERROR: We support kernel version 2.4.7 and higher.\n\n");
			flag_os_bad = 1;
			goto done;
		}
	}
  }
done:
  if (cmd) free(cmd);
}
#endif /* IDDS_LINUX_INCLUDE */


static void gen_tests (void)
{
  uid_t uid;

  if (flag_html) printf("<P>\n");

#if defined(__sun)
  if (flag_debug) printf("DEBUG  : /usr/bin/showrev -a\n");

  if (iii_pio_procparse("/usr/bin/showrev -a",
			III_PIO_SZ(ptb_showrev),
			ptb_showrev) == -1) {
    perror("/usr/bin/showrev -a");
  } else {
    int i;
    int pur = 0;

    for (i = 0; iii_patches[i].pnum != 0; i++) {
      if (iii_patches[i].sol == solaris_version && 
	  iii_patches[i].intel == flag_intel) {
	
	if (iii_patches[i].seen >= iii_patches[i].pver) {
	  if (flag_debug) {
	    printf("DEBUG  : Patch %d-%02d, %d-%02d required (%s)\n",
		   iii_patches[i].pnum,
		   iii_patches[i].seen,
		   iii_patches[i].pnum,
		   iii_patches[i].pver,
		   iii_patches[i].pdesc);
	  }
	} else if (iii_patches[i].seen != 0) {
	  if (flag_html) printf("<P>\n");
	  if (iii_patches[i].preq == 1 || flag_quick == 0) {
	    printf("%s: Patch %d-%02d is present, but %d-%02d (%s) is a more recent version.\n\n",
		   iii_patches[i].preq ? "ERROR " : "NOTICE ",
		   iii_patches[i].pnum,
		   iii_patches[i].seen,
		   iii_patches[i].pnum,
		   iii_patches[i].pver,
		   iii_patches[i].pdesc);
	    pur++;
	    if (flag_carrier || iii_patches[i].preq) flag_os_bad = 1;
	  }
	  if (flag_html) printf("</P>\n");
      } else {
	  if (flag_html) printf("<P>\n");
	  if (iii_patches[i].preq) {
	    printf("%s: Patch %d-%02d (%s) is required but not installed.\n\n",
		   "ERROR  ",
		   iii_patches[i].pnum,
		   iii_patches[i].pver,
		   iii_patches[i].pdesc);
	    flag_os_bad = 1;
	    pur++;
	  } else {
	    if (flag_quick == 0) {
	      printf("%s: Patch %d-%02d (%s) is not installed.\n\n",
		     "NOTICE ",
		     iii_patches[i].pnum,
		     iii_patches[i].pver,
		     iii_patches[i].pdesc);
	      pur++;
	    }
	  }
	  if (flag_html) printf("</P>\n");
	}
	
    } else if (iii_patches[i].seen) {
	if (flag_html) printf("<P>\n");
	printf("%s: Patch %d-%d seen on Solaris %d (%s) but intended for\nSolaris %d %s.\n\n",
	       "WARNING",
	       iii_patches[i].pnum,
	       iii_patches[i].seen,
	       solaris_version,
	       flag_intel ? "(Intel)" : "",
	       iii_patches[i].sol,
	       iii_patches[i].intel ? "(Intel)" : "");
	if (flag_html) printf("</P>\n");
	pur++;
	flag_warnings++;
      }

    } /* for */
    
    if (pur) {
      printf("NOTICE : Solaris patches can be obtained from http://sunsolve.sun.com or your\nSolaris support representative.  Solaris patches listed as required by the\nJRE are located at http://www.sun.com/software/solaris/jre/download.html or\ncan be obtained from your Solaris support representative.\n\n");
    }

  }

  sun_check_mu();
#endif

#if defined(IDDS_LINUX_INCLUDE)
  linux_check_release();
#endif


if (access("/usr/sbin/prtconf",X_OK) == 0) {
    if (flag_debug) printf("DEBUG  : /usr/sbin/prtconf\n");
    if (iii_pio_procparse("/usr/sbin/prtconf", III_PIO_SZ(ptb_prtconf),
			  ptb_prtconf) == -1) {
        perror("/usr/sbin/prtconf");
    }
}


#if defined(__hpux)
  hp_check_qpk();
  if (pstat_getdynamic(&pst_dyn,sizeof(pst_dyn),1,0) == -1 ||
      pstat_getstatic(&pst_stt,sizeof(pst_stt),1,0) == -1 ||
      pstat_getvminfo(&pst_vmm,sizeof(pst_vmm),1,0) == -1) {
    perror("pstat_getdynamic");
  } else {
    if (flag_debug) {
      printf("DEBUG  : Static info\n");
      printf("DEBUG  : Physical memory size %lu\n",pst_stt.physical_memory);
      printf("DEBUG  : Page size %lu\n",pst_stt.page_size);
      printf("DEBUG  : Max nfile %lu\n",pst_stt.pst_max_nfile);
      
      printf("DEBUG  : Dynamic info\n");
      printf("DEBUG  : Physical memory size %lu\n",pst_dyn.psd_rm);
      printf("DEBUG  : Virtual Memory size %lu\n",pst_dyn.psd_vm);
      printf("DEBUG  : Physical memory size %lu active\n",pst_dyn.psd_arm);
      printf("DEBUG  : Virtual Memory size %lu active\n",pst_dyn.psd_avm);
      printf("DEBUG  : Processors %lu\n",pst_dyn.psd_proc_cnt);

      printf("DEBUG  : VM Info\n");
      printf("DEBUG  : Pages on disk backing %lu\n",pst_vmm.psv_swapspc_cnt);
      printf("DEBUG  : Max pages on disk backing %lu\n",pst_vmm.psv_swapspc_max);
    }
    phys_mb = pst_stt.page_size / 1024;
    phys_mb = phys_mb * (pst_stt.physical_memory / 1024);
    swap_mb = pst_stt.page_size / 1024;
    swap_mb = swap_mb * (pst_vmm.psv_swapspc_cnt / 1024);
  }
#else
#if defined(IDDS_BSD_SYSCTL)
	/* phys_mb  from hw.physmem / 1048576 */
	/* swap_mb  from vm.stats.vm.v_page_count * v_page_size / 1048576 */
#endif
#endif
  
  if (flag_html) printf("<P>\n");

  if (phys_mb != 0 && phys_mb < mem_min) {
    printf("%s: Only %dMB of physical memory is available on the system. %dMB is the\nrecommended minimum. %dMB is recommended for best performance on large production system.\n\n","ERROR  ",phys_mb,
	   mem_min, mem_rec);
    flag_arch_bad = 1;
  } else if (phys_mb != 0 && phys_mb < mem_rec) {
    printf("%s: %dMB of physical memory is available on the system. %dMB is recommended for best performance on large production system.\n\n",
	   "WARNING",
	   phys_mb,mem_rec);
    flag_warnings++;
  } else if (flag_debug) {
    printf("DEBUG  : Memory size %d\n",phys_mb);
  }

  if (flag_html) printf("</P>\n");

  if (access("/usr/sbin/swap",X_OK) == 0) {
    if (flag_debug) printf("DEBUG  : /usr/sbin/swap -s\n");
    if (iii_pio_procparse("/usr/sbin/swap -s",
			  III_PIO_SZ(ptb_swap),
			  ptb_swap) == -1) {
      perror("/usr/sbin/swap -s");
    }
  }

#if defined(IDDS_LINUX_INCLUDE)
  if (1) {
    struct sysinfo linux_si;

    if (sysinfo(&linux_si) == 0) {
      swap_mb = linux_si.totalswap * (linux_si.mem_unit / 1024) / 1024;
    }
  }
#endif

  if (client == 0 && swap_mb < 0) {
    if (flag_html) printf("<P>\n");
    printf("%s: There is less swap space than physical memory.\n\n",
	   "ERROR  ");
    if (flag_html) printf("</P>\n");
    
  } else if (client == 0 && swap_mb && swap_mb < phys_mb) {
#if defined(__hpux) || defined(__sun)
#else
    if (flag_html) printf("<P>\n");
    printf("%s: There is %dMB of physical memory but only %dMB of swap space.\n\n",
	   "ERROR  ",
	   phys_mb,swap_mb);
    if (flag_html) printf("</P>\n");
#endif
  } else {
    if (flag_debug) printf("DEBUG  : %d MB swap configured\n", swap_mb);
  }

  uid = getuid();
  if (uid != 0) {
    uid = geteuid();
  }

#if defined(__sun)
  if (uid != 0) {
    if (flag_html) printf("<P>\n");
    if (flag_quick == 0) {
      printf("WARNING: This program should be run by the superuser to collect kernel\ninformation on the overriding maximum backlog queue size and IP tuning.\n\n");
      flag_warnings++;
    }
    flag_nonroot = 1;
    if (flag_html) printf("</P>\n");
  }
#endif


#if defined(__sun)  
  if (flag_quick == 0 && flag_nonroot == 0) {
    if (flag_debug) printf("DEBUG  : adb\n");
    if (iii_pio_procparse("/usr/bin/echo \"tcp_param_arr+14/D\" | /usr/bin/adb -k /dev/ksyms /dev/mem",
			  III_PIO_SZ(ptb_adb),
			  ptb_adb) == -1) {
      perror("adb");
    }
  }

  sun_check_etcsystem();
#endif

  if (uid != 0) {
    printf("\n");
  }

  if (flag_html) printf("</P>\n");
}



#if defined(__sun) || defined(__hpux) || defined(IDDS_LINUX_SYSCTL)

static int ndd_get_tcp (char *a,long *vPtr)
{
  char buf[8192];

#if defined(__sun)
  sprintf(buf,"/usr/sbin/ndd /dev/tcp %s",a);
#else
#if defined(__hpux)
  sprintf(buf,"/usr/bin/ndd /dev/tcp %s",a);
#else
#if defined(IDDS_LINUX_SYSCTL)
  sprintf(buf,"/sbin/sysctl -n %s",a);  	
#else
  sprintf(buf,"ndd /dev/tcp %s",a);
#endif
#endif
#endif
  
  if (flag_debug) printf("DEBUG  : %s\n",buf);
  if (iii_pio_getnum(buf,vPtr) == -1) {
    if (flag_solaris_251) {
      if (strcmp(a,"tcp_conn_req_max_q0") == 0 ||
	  strcmp(a,"tcp_conn_req_max_q") == 0 ||
	  strcmp(a,"tcp_slow_start_initial") == 0) {
	return -1;
      } 
    }

    printf("NOTICE : %s failed\n",buf);
    return -1;
  } 
  
  return 0;
}

#endif

#if defined(__sun)

static int patch_get_ver(int n)
{
  int i;
  
  for (i = 0;iii_patches[i].pnum != 0; i++) {
    if (iii_patches[i].pnum == n) {
      return iii_patches[i].seen;
    }
  }
  
  return 0;
}
#endif

#if defined(__hpux)

#include <dirent.h>
#define HP_PATCH_DIR "/var/adm/sw/products"

char *mo_lookup[] =
{
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December",
  NULL
};

static int month_lookup(char *month)
{
  int i;

  for (i = 0; mo_lookup[i]; i++)
  {
    if (!strcmp(month, mo_lookup[i]))
      return i+1;
  }
  return 0; /* error */
}

static void hp_check_index(char *index_path, char *desc, int yr, int mo)
{
  FILE *fp = fopen(index_path, "r");
  char buf[BUFSIZ];
  if (NULL == fp)
  {
    printf("ERROR: Failed to open Patch info file %s\n", index_path);
    return;
  }
  while (fgets(buf, BUFSIZ, fp))
  {
    if (!strncmp(buf, "title", 5))
    {
      char *p;
      char *datep = NULL;
      if (p = strstr(buf, desc))
      {
        if (NULL != p)
        {
          /* found */
          datep = strrchr(buf, ',');
		  if (!datep)
		  {
        /*    printf("WARNING: No date found: %s\n", datep);*/
			continue;
		  }
		  datep++;
          while (*datep == ' ' || *datep == '\t') datep++;
          p = strchr(datep, ' ');
          if (p)
          {
            char *q = p + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (isdigit(*q))
            {
			  char *qq;
              int my_year;
			  for (qq = q; qq && *qq && isdigit(*qq); qq++) ;
			  if (qq && *qq)
				*qq = '\0';
              my_year = atoi(q);
              if (my_year < yr)
              {
                printf("ERROR : %s, %s is older than the supported QPK of %s %d\n\n",
                       desc, datep, mo_lookup[mo-1], yr);
              }
              else if (my_year == yr)
              {
                int my_month;
                *p = '\0';
                my_month = month_lookup(datep);
                *p = ' ';
                if (my_month < mo)
                {
                  printf("ERROR : %s, %s is older than the supported QPK of %s %d\n\n",
                       desc, datep, mo_lookup[mo-1], yr);
                }
#ifdef QPK_DEBUG
				else
				{
                  printf("NOTICE: %s: Date %s is NO older than %d/%d\n",
                       desc, datep, mo, yr);
				}
#endif
              }
#ifdef QPK_DEBUG
			  else
			  {
                printf("NOTICE: %s: Date %s is NO older than %d/%d\n",
                       desc, datep, mo, yr);
			  }
#endif
            }
            else
            {
              printf("WARNING: Bad formatted date: %s\n", datep);
              flag_warnings++;
            }
          }
        }
      }
    }
  }

  fclose(fp);
}

static void hp_check_qpk()
{
  char fbuf[MAXPATHLEN];
  int i,pm= 0;
  int found = 0;

  DIR *prod_dir = NULL;
  struct dirent *dp = NULL;

  if (access(HP_PATCH_DIR,X_OK) == -1) {
    printf("\nWARNING : Only the superuser can check which patches are installed.  You must\nrun dsktune as root to ensure the necessary patches are present.  If required\npatches are not present, the server may not function correctly.\n\n");
    flag_warnings++;
    return;
  }

  for (i = 0; iii_qpk[i].qpk_name; i++)
  {
  
    prod_dir = opendir(HP_PATCH_DIR);
    if (!prod_dir)
    {
      printf("ERROR  : Patch directory %s has a problem.\n\n", HP_PATCH_DIR);
      return;
    }
    found = 0;
    while ((dp = readdir(prod_dir)) != NULL)
    {
	  int len = strlen(iii_qpk[i].qpk_name);
      if (strncmp(dp->d_name, iii_qpk[i].qpk_name, len) == 0)
      {
        /* matched */
	found=1;
        sprintf(fbuf, "%s/%s/pfiles/INDEX", HP_PATCH_DIR, dp->d_name);
        if (access(fbuf, R_OK) == -1)
        {
          printf("WARNING : Patch info file %s does not exist or not readable.\n\n", fbuf);
          flag_warnings++;
        }
        else
        {
          hp_check_index(fbuf, iii_qpk[i].qpk_desc, iii_qpk[i].qpk_yr, iii_qpk[i].qpk_mo);
        }
      }
    }
    if(found==0)
    {
      printf("ERROR : Patch %s (%s) was not found.\n\n",iii_qpk[i].qpk_name,iii_qpk[i].qpk_desc);
    }
    (void) closedir(prod_dir);
  }
}

static void hp_pthreads_tests(void)
{
  unsigned long tmax,omax;
  long cpu64;

  cpu64 = sysconf(_SC_HW_32_64_CAPABLE);

  if (_SYSTEM_SUPPORTS_LP64OS(cpu64) == 0) {
    printf("WARNING: This system does not support 64 bit operating systems.\n\n");
    flag_warnings++;
  } else {
    if (flag_debug) printf("DEBUG  : _SC_HW_32_64_CAPABLE 0x%lx\n",cpu64);
  }

  tmax = sysconf(_SC_THREAD_THREADS_MAX);

  if (tmax < 128) {
    printf("WARNING: only %lu threads are available in a process.\n", tmax);
    printf("NOTICE : use kmtune or sam Kernel Configuration Parameters to change max_thread_proc\n");
    printf("and nkthreads as needed.\n\n");
    flag_warnings++;
  } else {
    if (flag_debug) printf("DEBUG  : HP-UX max threads %lu\n", tmax);
  }

  /* XXX set ncallout (max number of pending timeouts ) to 128 + NPROC */


  /* set maxfiles to at least 120 */
  omax = sysconf(_SC_OPEN_MAX);
  
  if (omax < 120) {
    printf("WARNING: only %lu files can be opened at once in a process.\n",omax);
    printf("NOTICE : use kmtune or sam Kernel Configuration Parameters to change maxfiles.\n");    
    flag_warnings++;
  } else {
    if (flag_debug) printf("DEBUG  : HP-UX maxfiles %ld\n", omax);    
  }

}
#endif

#if defined(__sun)
static void sun_check_network_device(void)
{
  int devfd;
  char buf[8192];
  long ls;

  if (flag_intel || flag_arch_bad || flag_os_bad) return;

  devfd = open(SUN_NETWORK_DEVICE,O_RDONLY);
  
  if (devfd == -1) {
    switch (errno) {

    case EACCES:
      if (flag_debug) printf("DEBUG  : got EACCES opening %s\n",
                 SUN_NETWORK_DEVICE);
      break;
    case ENOENT:
      if (flag_debug) printf("DEBUG  : got ENOENT opening %s\n",
                 SUN_NETWORK_DEVICE);
      break;
    default:
      if (flag_debug) printf("DEBUG  : got %d opening %s\n",
                 errno,SUN_NETWORK_DEVICE);
    }
    return;
  } else {
    close(devfd);
  }
    
  sprintf(buf,"/usr/sbin/ndd %s link_speed",SUN_NETWORK_DEVICE);
  if (flag_debug) printf("DEBUG  : %s\n",buf);
  if (iii_pio_getnum(buf,&ls) == -1) {
    if (flag_debug) printf("DEBUG  : %s link_speed variable not available\n",
               SUN_NETWORK_DEVICE);
  } else {
    /* XXX look at link speed */
    if (flag_debug) printf("DEBUG  : %s link_speed is %d\n",
               SUN_NETWORK_DEVICE,ls);
  }
}
#endif

#if defined(__sun) || defined(__hpux)|| defined(IDDS_LINUX_SYSCTL)

static void ndd_tests (void)
{
  if (flag_html) printf("<P>\n");

#if defined(IDDS_LINUX_SYSCTL)
  /* following linux sysctls are TBD:
   net.ipv4.tcp_max_syn_backlog, net.ipv4.tcp_fin_timeout
   tcp_retries2 and tcp_retries
   */
#endif

#if !defined(IDDS_BSD_SYSCTL) && !defined(IDDS_LINUX_SYSCTL)
  {
    char *name_tcp_time_wait_interval;
    if (!flag_solaris_26) {
      name_tcp_time_wait_interval = NAME_TCP_TIME_WAIT_INTERVAL;
    } else {
      name_tcp_time_wait_interval = "tcp_close_wait_interval";
    }

    if (ndd_get_tcp(name_tcp_time_wait_interval, &ndd_tcp_time_wait_interval) == 0) {
      if (ndd_tcp_time_wait_interval >= 240000) {
        if (flag_html) printf("<P>\n");
        printf("%s: The %s is set to %ld milliseconds (%ld seconds).\n"
		  "This value should be reduced to allow for more simultaneous connections\n"
		  "to the server.\n",
	       flag_carrier ? "ERROR  " : "WARNING",
	       name_tcp_time_wait_interval,
	       ndd_tcp_time_wait_interval,
	       ndd_tcp_time_wait_interval/1000);
#ifdef NAME_NDD_CFG_FILE
        printf("An entry similar to the following\nshould be added to the %s file:\n", NAME_NDD_CFG_FILE);
        if (flag_html) printf("</P><PRE>\n");
        printf("TRANSPORT_NAME[10]=tcp\n");
        printf("NDD_NAME[10]=%s\n", name_tcp_time_wait_interval);
        printf("NDD_VALUE[10]=%d\n\n", 30000);
        if (flag_html) printf("</PRE><P>\n");
#endif
        if (flag_carrier) {
          flag_os_bad = 1;
        } else {
          flag_warnings++;
        }
      } else if (ndd_tcp_time_wait_interval < 10000) {
        if (flag_html) printf("<P>\n");
        printf("WARNING: The %s is set to %ld milliseconds.  Values below\n30000 may cause problems.\n\n",
	  name_tcp_time_wait_interval, ndd_tcp_time_wait_interval);
        if (flag_html) printf("</P>\n");
        flag_warnings++;
      } else {
        if (flag_debug) {
	  printf("DEBUG  : %s %ld\n", name_tcp_time_wait_interval, ndd_tcp_time_wait_interval);
        }
      }
    }
  }

#if defined(__sun)
  if (client == 0) {
    if (ndd_get_tcp("tcp_conn_req_max_q0",&ndd_tcp_conn_req_max_q0) == 0) {
      
      if (ndd_tcp_conn_req_max_q0 < 1024) {
	if (flag_html) printf("<P>\n");      
	printf("ERROR  : The tcp_conn_req_max_q0 value is too low, %d.\n\n",
	       ndd_tcp_conn_req_max_q0);
	if (flag_solaris_251) {
	  printf("ERROR  : Patches %s and %s may need to be applied.\n\n",
		 flag_intel ? "103631-10" : "103630-13",
		 flag_intel ? "103581-18" : "103582-18");
	}
	if (flag_html) printf("</P>\n");      
      } else if (ndd_tcp_conn_req_max_q0 >= (flag_carrier ? 10240 : 1024)) {
	if (flag_debug) {
	  printf("DEBUG  : tcp_conn_req_max_q0 %d\n",
		 ndd_tcp_conn_req_max_q0);
	}
      } else {
	if (flag_html) printf("<P>\n");      
	printf("WARNING: The tcp_conn_req_max_q0 value is currently %d, which will limit the\nvalue of listen backlog which can be configured.  It can be raised by adding\nto /etc/init.d/inetinit, after any adb command, a line similar to:\n",
	       ndd_tcp_conn_req_max_q0);
	if (flag_html) printf("</P><PRE>\n");      
	printf("ndd -set /dev/tcp tcp_conn_req_max_q0 65536\n");
	if (flag_html) printf("</PRE><P>\n");      
	if (tcp_max_listen == 1024) {
	  printf("Raising this to a value larger than 1024 may require that adb be used first\nto change the maximum setting.\n"); 
	}
	if (flag_html) printf("</P>\n");      
	
	printf("\n");
	flag_warnings++;
      }
      
      if (tcp_max_listen && ndd_tcp_conn_req_max_q0 > tcp_max_listen) {
	if (flag_html) printf("<P>\n");      
	printf("WARNING: tcp_conn_req_max_q0 is larger than the kernel will allow.\n\n");
	if (flag_html) printf("</P>\n");      
      }
      
    } else {
      if (flag_solaris_251) {
	if (flag_html) printf("<P>\n");      
	printf("ERROR  : Solaris tuning parameters were improved in patch %s for\nSolaris 2.5.1 which introduces a fix for the SYN flood attack.  Installing\nthis patch is strongly recommended.\n\n",
	       flag_intel ? "103581-18" : "103582-18");
	if (flag_html) printf("</P>\n");      
	
      }
    }
  }
#endif
  
  if (client == 0) {
    int recommended_tcp_conn_req_max = 128;

    if (ndd_get_tcp(NAME_TCP_CONN_REQ_MAX_Q, &ndd_tcp_conn_req_max_q) == 0) {
      if (flag_html) printf("<P>\n");      
      if (ndd_tcp_conn_req_max_q < recommended_tcp_conn_req_max) {
	printf("ERROR  : The NDD %s value %ld is lower than the recommended minimum, %d.\n\n",
	       NAME_TCP_CONN_REQ_MAX_Q, ndd_tcp_conn_req_max_q, recommended_tcp_conn_req_max);
	if (flag_solaris_251) {
	  printf("ERROR  : Patches %s and %s may need to be applied.\n\n",
		 flag_intel ? "103631-10" : "103630-13",
		 flag_intel ? "103581-18" : "103582-18");
	}
      } else if (ndd_tcp_conn_req_max_q >= ndd_tcp_conn_req_max_q0) {
	if (flag_debug) {
	  printf("DEBUG  : %s %ld\n", NAME_TCP_CONN_REQ_MAX_Q, ndd_tcp_conn_req_max_q);
	}
      } else {
	printf("NOTICE : The %s value is currently %ld, which will limit the\nvalue of listen backlog which can be configured.  ",
	       NAME_TCP_CONN_REQ_MAX_Q, ndd_tcp_conn_req_max_q);
#ifdef NAME_NDD_CFG_FILE
	printf("It can be raised by adding\nto %s, after any adb command, an entry similar to:\n", NAME_NDD_CFG_FILE);
	if (flag_html) printf("</P><PRE>\n");
	printf("TRANSPORT_NAME[10]=tcp\n");
	printf("NDD_NAME[10]=%s\n", NAME_TCP_CONN_REQ_MAX_Q);
	printf("NDD_VALUE[10]=%ld\n\n", ndd_tcp_conn_req_max_q0);
	if (flag_html) printf("</PRE><P>\n");      
#endif
	if (tcp_max_listen == 1024) {
	  printf("Raising this to a value larger than 1024 may require that adb be used first\nto change the maximum setting.\n"); 
	}
	printf("\n");
      }
      if (flag_html) printf("</P><P>\n");      
      
      if (tcp_max_listen && ndd_tcp_conn_req_max_q > tcp_max_listen) {
	printf("WARNING: %s (value %ld) is larger than the kernel will allow.\n\n", NAME_TCP_CONN_REQ_MAX_Q, ndd_tcp_conn_req_max_q);
	flag_warnings++;
      }
      
      if (flag_html) printf("</P><P>\n");      
      
    } else {
      if (flag_solaris_251) {
	if (flag_html) printf("</P><P>\n");      
	printf("ERROR  : Solaris tuning parameters were improved in patch %s for\nSolaris 2.5.1 which introduces a fix for the SYN flood attack.  Installing\nthis patch is strongly recommended.\n\n",
	       flag_intel ? "103581-18" : "103582-18");
	
	if (flag_html) printf("</P><P>\n");      
      }
    }
  }
#endif
  /* end of Solaris/HP-only code */

  if (client == 0) {
    if (ndd_get_tcp(NAME_TCP_KEEPALIVE_INTERVAL, &ndd_tcp_keepalive_interval) == 0) {

#if defined(IDDS_LINUX_SYSCTL)
      ndd_tcp_keepalive_interval *= 1000;  /* seconds to milliseconds */
#endif

      if (ndd_tcp_keepalive_interval) {
	if (solaris_version == 25 || solaris_version == 24) {
	  if (flag_html) printf("</P><P>\n");      
	  printf("ERROR  : The %s should not be set on versions of Solaris\nprior to 2.6 as thery contain a bug that causes infinite transmission.\n\n",NAME_TCP_KEEPALIVE_INTERVAL);
	  if (flag_html) printf("</P><P>\n");      
	} else if (flag_solaris_251) {
	  if (flag_html) printf("</P><P>\n");      
	  printf("WARNING: There may be a bug in Solaris 2.5.1 which causes infinite\nretransmission when the %s (%ld s) is set.  As there is\nno known fix, upgrading to Solaris 2.6 or later is recommended.\n\n",
		 NAME_TCP_KEEPALIVE_INTERVAL, ndd_tcp_keepalive_interval/1000);
	  if (flag_html) printf("</P><P>\n");      
	  flag_warnings++;
	} else {
	  if (ndd_tcp_keepalive_interval < 60000) {
	    if (flag_html) printf("</P><P>\n");      
	    printf("NOTICE : The %s is set to %ld milliseconds\n(%ld seconds).  This may cause excessive retransmissions in WAN\nenvironments.\n\n",
		   NAME_TCP_KEEPALIVE_INTERVAL,
		   ndd_tcp_keepalive_interval,
		   ndd_tcp_keepalive_interval/1000);
	    if (flag_html) printf("</P><P>\n");      
	  } else if (ndd_tcp_keepalive_interval > 600000) {
	    if (flag_html) printf("</P><P>\n");      
	    printf("NOTICE : The %s is set to %ld milliseconds\n(%ld minutes).  This may cause temporary server congestion from lost\nclient connections.\n\n",
		   NAME_TCP_KEEPALIVE_INTERVAL,
		   ndd_tcp_keepalive_interval,
		   ndd_tcp_keepalive_interval / 60000);
	    if (flag_html) printf("</P><P>\n");      
#ifdef NAME_NDD_CFG_FILE
	    printf("An entry similar to the following should be added to %s:\n", NAME_NDD_CFG_FILE);
	    if (flag_html) printf("</P><PRE>\n");
	    printf("TRANSPORT_NAME[10]=tcp\n");
	    printf("NDD_NAME[10]=%s\n", NAME_TCP_KEEPALIVE_INTERVAL);
	    printf("NDD_VALUE[10]=%d\n\n", 600000);
	    if (flag_html) printf("</PRE><P>\n");
#endif
	  } else if (flag_debug) {
	    printf("DEBUG  : %s %ld (%ld seconds)\n",
		   NAME_TCP_KEEPALIVE_INTERVAL,
		   ndd_tcp_keepalive_interval,
		   ndd_tcp_keepalive_interval / 1000);
	  }
	}
      } else {
	if (flag_solaris_251) {
	  if (flag_html) printf("</P><P>\n");      
	  printf("NOTICE : The %s is currently not set.  Setting this value\nshould only be done on Solaris 2.6 or later, due to a bug in earlier versions\nof Solaris.\n\n",NAME_TCP_KEEPALIVE_INTERVAL);
	  if (flag_html) printf("</P><P>\n");      
	} else {
	  if (flag_html) printf("</P><P>\n");      
#ifdef NAME_NDD_CFG_FILE
	  printf("NOTICE : The %s is currently not set.  This could result in\neventual server congestion.  The interval can be set by adding an entry similar to the following to %s:\n",NAME_TCP_KEEPALIVE_INTERVAL, NAME_NDD_CFG_FILE);
	  if (flag_html) printf("</P><PRE>\n");      
	  printf("TRANSPORT_NAME[10]=tcp\n");
	  printf("NDD_NAME[10]=%s\n", NAME_TCP_KEEPALIVE_INTERVAL);
	  printf("NDD_VALUE[10]=%d\n\n", 60000);
	  if (flag_html) printf("</PRE><P>\n");      
#endif
	  printf("\n");
      }
      }
    }
  }

#if !defined(IDDS_LINUX_SYSCTL)
  if (ndd_get_tcp("tcp_rexmit_interval_initial",
		  &ndd_tcp_rexmit_interval_initial) == 0) {
    if (ndd_tcp_rexmit_interval_initial > 2000) {
      if (flag_html) printf("</P><P>\n");      
      printf("NOTICE : The NDD tcp_rexmit_interval_initial is currently set to %ld\nmilliseconds (%ld seconds).  This may cause packet loss for clients on\nSolaris 2.5.1 due to a bug in that version of Solaris.  If the clients\nare not using Solaris 2.5.1, no problems should occur.\n\n",
	     ndd_tcp_rexmit_interval_initial,
	     ndd_tcp_rexmit_interval_initial/1000);
      if (flag_html) printf("</P><P>\n");      
#ifdef NAME_NDD_CFG_FILE
      if (client) {
	printf("NOTICE : For testing on a LAN or high speed WAN, this interval can be reduced\n"
		"by adding an entry similar to the following to %s file:\n", NAME_NDD_CFG_FILE);
      } else {
	printf("NOTICE : If the directory service is intended only for LAN or private \n"
		"high-speed WAN environment, this interval can be reduced by adding an\n"
		"entry similar to the following to %s file:\n", NAME_NDD_CFG_FILE);
      }
      if (flag_html) printf("</P><PRE>\n");      
      printf("TRANSPORT_NAME[10]=tcp\n");
      printf("NDD_NAME[10]=tcp_rexmit_interval_initial\n");
      printf("NDD_VALUE[10]=%d\n\n", 500);
      if (flag_html) printf("</PRE><P>\n");      
#endif
    } else {
      if (flag_html) printf("</P><P>\n");      
      printf("NOTICE : The tcp_rexmit_interval_initial is currently set to %ld\n"
	"milliseconds (%ld seconds).  This may cause excessive retransmission on the\n"
	"Internet.\n\n",
	     ndd_tcp_rexmit_interval_initial,
	     ndd_tcp_rexmit_interval_initial/1000);
      if (flag_html) printf("</P><P>\n");      
    }
  }
#endif

#if !defined(IDDS_LINUX_SYSCTL)
  if (ndd_get_tcp("tcp_ip_abort_cinterval", &ndd_tcp_ip_abort_cinterval) == 0) {
    if (ndd_tcp_ip_abort_cinterval > 10000) {
      if (flag_html) printf("</P><P>\n");      
      printf("NOTICE : The NDD tcp_ip_abort_cinterval is currently set to %ld\n"
	"milliseconds (%ld seconds).  This may cause long delays in establishing\n"
	"outgoing connections if the destination server is down.\n\n",
	     ndd_tcp_ip_abort_cinterval,
	     ndd_tcp_ip_abort_cinterval/1000);
      if (flag_html) printf("</P><P>\n");      
#ifdef NAME_NDD_CFG_FILE
      printf("NOTICE : If the directory service is intended only for LAN or private \n"
	"high-speed WAN environment, this interval can be reduced by adding an entry\n"
	"similar to the following to %s file:\n", NAME_NDD_CFG_FILE);
      if (flag_html) printf("</P><PRE>\n");      
      printf("TRANSPORT_NAME[10]=tcp\n");
      printf("NDD_NAME[10]=tcp_ip_abort_cinterval\n");
      printf("NDD_VALUE[10]=%d\n\n", 10000);
      if (flag_html) printf("</PRE><P>\n");      
#endif
    }
  }
  if (ndd_get_tcp("tcp_ip_abort_interval", &ndd_tcp_ip_abort_interval) == 0) {
    if (ndd_tcp_ip_abort_cinterval > 60000) {
      if (flag_html) printf("</P><P>\n");      
      printf("NOTICE : The NDD tcp_ip_abort_interval is currently set to %ld\nmilliseconds (%ld seconds).  This may cause long delays in detecting\nconnection failure if the destination server is down.\n\n",
	     ndd_tcp_ip_abort_cinterval,
	     ndd_tcp_ip_abort_cinterval/1000);
      if (flag_html) printf("</P><P>\n");      
#ifdef NAME_NDD_CFG_FILE
      printf("NOTICE : If the directory service is intended only for LAN or private \nhigh-speed WAN environment, this interval can be reduced by adding an entry\nsimilar to the following to %s:\n", NAME_NDD_CFG_FILE);
      if (flag_html) printf("</P><PRE>\n");      
      printf("TRANSPORT_NAME[10]=tcp\n");
      printf("NDD_NAME[10]=tcp_ip_abort_interval\n");
      printf("NDD_VALUE[10]=%d\n\n", 60000);
      if (flag_html) printf("</PRE><P>\n");      
#endif
    }
  }
#endif

#if defined(__sun)
  if (ndd_get_tcp("tcp_strong_iss",
		  &ndd_tcp_strong_iss) == 0) {
    switch(ndd_tcp_strong_iss) {
    case 0:
      if (flag_debug) printf("DEBUG  : tcp_strong_iss 0\n");
      break;
    case 1:
      if (flag_debug) printf("DEBUG  : tcp_strong_iss 1\n");
      printf("NOTICE : The TCP initial sequence number generation is not based on RFC 1948.\nIf this directory service is intended for external access, add the following\nto /etc/init.d/inetinit:\n");  
      if (flag_html) printf("</P><PRE>\n");      
      printf("ndd -set /dev/tcp tcp_strong_iss 2\n\n");
      if (flag_html) printf("</PRE><P>\n");      
      break;
    case 2:
      if (flag_debug) printf("DEBUG  : tcp_strong_iss 2\n");
      break;
    }
  } else {
    if (flag_debug) printf("DEBUG  : tcp_strong_iss not found\n");
  }
#endif

  /* Linux uses net.ipv4.ip_local_port_range = 1024     4999 */
#if !defined(IDDS_LINUX_SYSCTL)
  if (ndd_get_tcp(NAME_TCP_SMALLEST_ANON_PORT,
		  &ndd_tcp_smallest_anon_port) == 0) {
    if (ndd_tcp_smallest_anon_port >= 32768) {
      int aport = 65536-ndd_tcp_smallest_anon_port;
      
      if (flag_html) printf("</P><P>\n");      
      printf("%s: The NDD %s is currently %ld.  This allows a\nmaximum of %ld simultaneous connections.  ",
	     (flag_carrier || aport <= 16384) ? "ERROR  " : "NOTICE ",
	     NAME_TCP_SMALLEST_ANON_PORT,
	     ndd_tcp_smallest_anon_port,
	     65536 - ndd_tcp_smallest_anon_port);
      if (flag_carrier) flag_os_bad = 1;
#ifdef NAME_NDD_CFG_FILE
      printf("More ports can be made available by\nadding an entry similar to\nthe following to %s:\n", NAME_NDD_CFG_FILE);
      if (flag_html) printf("</P><PRE>\n");      
      printf("TRANSPORT_NAME[10]=tcp\n");
      printf("NDD_NAME[10]=tcp_smallest_anon_port\n");
      printf("NDD_VALUE[10]=%d\n\n", 8192);
      if (flag_html) printf("</PRE><P>\n");      
#endif
      printf("\n");
    } else {
      if (flag_debug) {
	printf("DEBUG  : %s %ld\n", NAME_TCP_SMALLEST_ANON_PORT,
	       ndd_tcp_smallest_anon_port);
      }
    }
  }
#endif

#if defined(__sun)
  if (ndd_get_tcp("tcp_slow_start_initial",&ndd_tcp_slow_start_initial) == 0) {
    if (ndd_tcp_slow_start_initial == 2 || ndd_tcp_slow_start_initial == 4) {
      if (flag_debug) printf("DEBUG  : tcp_slow_start_initial %ld\n",
			     ndd_tcp_slow_start_initial);
    } else if (ndd_tcp_slow_start_initial == 1) {
      if (client == 0) {
	if (flag_html) printf("</P><P>\n");      
	printf("NOTICE : tcp_slow_start_initial is currently 1.  If clients are running on\nWindows TCP/IP stack, improved performance may be obtained by changing this\nvalue to 2.");
	
	printf("This line can be added to the /etc/init.d/inetinit file:\n");
	if (flag_html) printf("</P><PRE>\n");      
	printf("ndd -set /dev/tcp tcp_slow_start_initial 2\n");
	if (flag_html) printf("</PRE><P>\n");      
	printf("\n");
      }
    } else {
      printf("NOTICE : Unrecognized tcp_slow_start_initial value %ld\n\n",
	     ndd_tcp_slow_start_initial);
    }
    
    
  } else {
    if (flag_solaris_251) {
      int cpv;
      
      cpv = patch_get_ver(flag_intel ? 103581: 103582);

      if (flag_html) printf("</P><P>\n");            
      printf("ERROR  : Solaris tuning parameters were improved in patch %s for\nSolaris 2.5.1 which introduces the tcp_slow_start_initial variable. Installing\n%spatch is strongly recommended.\n\n",
	     flag_intel ? "103581-15" : "103582-15",
	     cpv == 0 ? "this or a later version of this " : "a later version of this ");
      if (flag_html) printf("</P><P>\n");            
    }
  }
#endif

#if defined(IDDS_BSD_SYSCTL)
  if (1) {
    if (ndd_get_tcp("net.inet.tcp.delayed_ack",&ndd_tcp_deferred_ack_interval) == 0) {
      if (ndd_tcp_deferred_ack_interval > 0) {
	printf("WARNING: net.inet.tcp.delayed_ack is currently set. This will\ncause FreeBSD to insert artificial delays in the LDAP protocol.  It should\nbe reduced during load testing.\n");
	flag_warnings++;
      } else {
	if (flag_debug) printf("DEBUG  : ndd /dev/tcp tcp_deferred_ack_interval %ld\n", ndd_tcp_deferred_ack_interval);
      }
    }
  }
#endif

#if defined(__sun) || defined(__hpux)
  if (1) {
    if (ndd_get_tcp("tcp_deferred_ack_interval",&ndd_tcp_deferred_ack_interval) == 0) {
      if (ndd_tcp_deferred_ack_interval > 5) {
	printf("%s: tcp_deferred_ack_interval is currently %ld milliseconds. This will\ncause the operating system to insert artificial delays in the LDAP protocol.  It should\nbe reduced during load testing.\n", 
	       flag_carrier ? "ERROR  " : "WARNING",
	       ndd_tcp_deferred_ack_interval);
	if (flag_carrier) {
	  flag_os_bad = 1;
	} else {
	  flag_warnings++;
	}
#ifdef NAME_NDD_CFG_FILE
      printf("An entry similar to the following can be\nadded to the %s file:\n", NAME_NDD_CFG_FILE);
      if (flag_html) printf("</P><PRE>\n");      
      printf("TRANSPORT_NAME[10]=tcp\n");
      printf("NDD_NAME[10]=tcp_deferred_ack_interval\n");
      printf("NDD_VALUE[10]=%d\n\n", 5);
      if (flag_html) printf("</PRE><P>\n");      
#endif
      printf("\n");
      } else {
	if (flag_debug) printf("DEBUG  : ndd /dev/tcp tcp_deferred_ack_interval %ld\n", ndd_tcp_deferred_ack_interval);
      }
    }
  }
#endif

  /* must be root to see 
   * ip_forward_src_routed, ip_forward_directed_broadcasts,
   * ip_forwarding XXX
   */
  
#if defined(__sun)
  sun_check_network_device();
#endif

#if !defined(IDDS_LINUX_SYSCTL)
  if (hpux_ndd_change_needed) {
    printf("NOTICE : ndd settings can be placed in /etc/rc.config.d/nddconf\n\n");
  }
#endif

  if (flag_html) printf("</P>\n");

}

#endif

static int get_disk_avail(char *dir)
{
 
#if defined(__sun)
  char cmd[8192];
  FILE *fp;
  char buf[8192];

  if (client) return 0;

  sprintf(cmd,"df -b %s",dir);
  
  if (flag_debug) printf("DEBUG  : %s\n",cmd);
  fp = popen(cmd,"r");

  if (fp == NULL) {
    perror("popen");
    return -1;
  }

  while (fgets(buf,8192,fp) != NULL) {
    char *rp;
    int i;

    rp = strchr(buf,'\n');
    
    if (rp) {
      *rp = '\0';
    }
    
    if (strncmp(buf,"Filesystem",10) == 0) {
      continue;
    }
    if (strncmp(buf,"df: ",4) == 0) {
      printf("ERROR  : %s\n\n", buf);
      fclose(fp);
      return -1;
    }
    rp = strchr(buf,':');
    if (rp) {
      *rp = '\0';
      if (flag_html) printf("<P>\n");
      printf("ERROR  : %s partition is on file system mounted from %s, not local.\n\n",dir,buf);
      if (flag_html) printf("</P>\n");
      fclose(fp);
      return -1;
    }
    
    rp = strchr(buf,' ');
    if (rp == NULL) {
      rp = strchr(buf,'\t');
    }
    if (rp == NULL) continue;
    
    while(isspace(*rp)) rp++;
    
    if (!isdigit(*rp)) {
      continue;
    }
    i = atoi(rp);
    fclose(fp);
    return (i / 1024);
  }
  fclose (fp);
#else
#if defined(IDDS_HAVE_STATVFS)
  struct statvfs vfs;

  if (statvfs(dir,&vfs) == 0) {
    return ((vfs.f_bfree * (vfs.f_bsize / 1024)) / 1024);
  }
#else
#if defined(IDDS_LINUX_INCLUDE)
  struct statfs sfs;

  if (statfs(dir,&sfs) == 0) {
    return ((sfs.f_bfree * (sfs.f_bsize / 1024)) / 1024);
  }
#endif
#endif
#endif
  return -1;
}

/* return 0 if fsmnt is a longer subset of reqdir than mntbuf */
static int mntdir_matches(char *reqdir,int reqlen,char *fsmnt,char *mntbuf)
{
  int cml;
  int pml;

  cml = strlen(fsmnt);
  pml = strlen(mntbuf);

  /* incoming file system is 'below' */
  if (reqlen < cml) {
    if (flag_debug) printf("DEBUG  : mntdir_matches want %s < input %s\n", 
			   reqdir, fsmnt);
    return -1;
  }
  
  if (reqlen == cml && strcmp(reqdir,fsmnt) == 0) {
    /* exact match */
    if (flag_debug) printf("DEBUG  : reqlen %d == cml %d\n", reqlen, cml);
    strcpy(mntbuf,fsmnt);
    return 0;
  }

  /* assert reqlen >= cml */

  if (strncmp(fsmnt,reqdir,cml) != 0) {
    if (flag_debug) printf("DEBUG  : fsmnt %s != reqdir %s\n", fsmnt, reqdir);
    return -1;
  }

  if (reqdir[cml] != '/' && cml > 1) {
    if (flag_debug) printf("DEBUG  : fsmnt %s: reqdir %s is %c not / \n", 
			   fsmnt, reqdir, reqdir[cml]);
    return -1;
  }
  
  if (pml > cml) {
    if (flag_debug) printf("DEBUG  : pml %d > cml %d\n", pml, cml);
    return -1;
  }

  if (flag_debug) printf("DEBUG  : replacing %s with %s\n", mntbuf, fsmnt);
  strcpy(mntbuf,fsmnt);
  
  return 0;
}

/* check that the file system has largefiles on HP */

static int check_fs_options(char *reqdir,char mntbuf[MAXPATHLEN])
{
#if defined(IDDS_MNTENT_DIRNAME)
  FILE *fp = NULL;
  int found = -1;
  int any_found = 0;
  int reqlen = strlen(reqdir);
  char optbuf[BUFSIZ];

  if (client == 1) return -1;
  
  mntbuf[0] = '\0';
  optbuf[0] = '\0';

#if defined(IDDS_MNTENT_MNTTAB)
  fp = fopen(IDDS_MNTENT_MNTTAB,"r");
  
  if (fp == NULL) {
    perror(IDDS_MNTENT_MNTTAB);
    return -1;
  }
#endif
  
  while(1) {
    struct IDDS_MNTENT *mep;

#if defined(__sun)
    struct IDDS_MNTENT m;
    
    mep = &m;
    if (getmntent(fp,mep) != 0) break;
#else
#if defined(__hpux) || defined(IDDS_LINUX_INCLUDE)
    mep = getmntent(fp);
    if (mep == NULL) break;
#else
#if !defined(IDDS_MNTENT_MNTTAB)
    /* not quite the same, but Tru64 and AIX don't have getmntent */
    mep = getfsent();
    if (mep == NULL) break;
#else
    break;
#endif
#endif
#endif
    
    if (mntdir_matches(reqdir,reqlen,mep->IDDS_MNTENT_DIRNAME,mntbuf) == 0) {
      found = 0;
#if defined(IDDS_MNTENT_OPTIONS)
      strncpy(optbuf,mep->IDDS_MNTENT_OPTIONS,sizeof(optbuf));
      optbuf[sizeof(optbuf)-1] = '\0';
#else
      strcpy(optbuf,"");
#endif

      if (flag_debug) printf("DEBUG  : file system %s matches %s with options %s\n",
			     mntbuf, reqdir, optbuf);
    } else {
#if defined(IDDS_MNTENT_OPTIONS)
      if (strstr(mep->IDDS_MNTENT_OPTIONS,"nolargefiles") != NULL) {

      } else if (strstr(mep->IDDS_MNTENT_OPTIONS,"largefiles") != NULL) {
	if (flag_debug) printf("DEBUG  : file system %s allows largefiles\n",
			       mep->IDDS_MNTENT_DIRNAME);
	any_found++;
      }
#endif
    }
    
  }

  if (fp) fclose (fp);
  
#if defined(__hpux)
  if (found == 0) {
    int largefile_missing = 0;

    if (strstr(optbuf,"nolargefiles") != NULL) {
      largefile_missing = 1;
    } else if (strstr(optbuf,"largefiles") == NULL) {
      largefile_missing = 1;
    }

    if (largefile_missing) {
      if (any_found == 0) {
	printf("WARNING: largefiles option is not present on mount of %s, \nfiles may be limited to 2GB in size.\n\n", mntbuf);
      } else {
	printf("WARNING: largefiles option is not present on mount of %s, \nalthough it is present on other file systems.  Files on the %s\nfile system will be limited to 2GB in size.\n\n", mntbuf, mntbuf);	
      }
      flag_warnings++;
    }
  } else {
    if (any_found == 0) {
      printf("WARNING: no file system mounted with largefiles option.\n\n");
      flag_warnings++;
    }
  }
#endif


  return found;
  
#else 
  return -1;
#endif 
}

static void check_disk_quota(char mntbuf[MAXPATHLEN])
{
#if defined(__sun)
  char qfname[MAXPATHLEN];
  struct stat sbuf;

  sprintf(qfname,"%s/quotas",mntbuf);
  if (stat(qfname,&sbuf) == 0 || errno == EACCES) {
    printf("NOTICE : quotas are present on file system %s.\n\n",mntbuf); 
  }

#endif
}

static void disk_tests(void)
{
  struct rlimit r;
  char mntbuf[MAXPATHLEN];

  if (client) return;

  avail_root = get_disk_avail("/");
  if (flag_debug) printf("DEBUG  : %dMB available on /\n",avail_root);
  
  if (flag_html) printf("<P>\n");

  if (avail_root != -1 && avail_root < 2) {
  if (flag_html) printf("</P><P>\n");
    printf("ERROR  : / partition is full\n");
    flag_os_bad = 1;
    if (flag_html) printf("</P><P>\n");
  }

#if defined(RLIMIT_CORE)
  getrlimit(RLIMIT_CORE,&r);
  if (flag_debug) printf("DEBUG  : RLIMIT_CORE is %ld, %ld\n", r.rlim_cur, r.rlim_max);
  if (r.rlim_cur == (unsigned long)-1 || r.rlim_cur >= 2147483647) {
    if (swap_mb <2048) {
      max_core = swap_mb;
    } else {
      max_core = 2048;
    }
  } else {
    max_core = r.rlim_max / (1024*1024);
  }
  if (phys_mb) {
    if (max_core > (phys_mb + swap_mb)) {
      max_core = phys_mb + swap_mb;
    }
  }
#endif
  
  if (avail_root != -1 && max_core > avail_root && flag_quick == 0) {
    if (flag_html) printf("</P><P>\n");
    printf("NOTICE : / partition has less space available, %dMB, than the largest \nallowable core file size of %dMB.  A daemon process which dumps core could\ncause the root partition to be filled.\n\n",
	   avail_root, max_core);
    if (flag_html) printf("</P><P>\n");
  }

  if (install_dir[0] == '\0') {
    if (access("/usr/" VENDOR,X_OK) == 0) {
      sprintf(install_dir,"/usr/" VENDOR);      
    } else {
      sprintf(install_dir,"/opt");
    }
  }
  
  if (check_fs_options(install_dir,mntbuf) == 0) {
    
  } else {
    strcpy(mntbuf,install_dir);
  }

  avail_opt = get_disk_avail(mntbuf);
  if (flag_debug) printf("DEBUG  : %dMB available on %s\n",
			 avail_opt,mntbuf);
  
  if (avail_opt != -1) {
    if (flag_html) printf("</P><P>\n");
    if (avail_opt < 2) {
      printf("ERROR  : %s partition is full.\n",mntbuf);
    } else if (avail_opt < 100) {
      printf("NOTICE : %s partition has only %dMB free.\n",mntbuf,avail_opt);  
    }
    if (flag_html) printf("</P><P>\n");
  }


  check_disk_quota(mntbuf);

  if (flag_html) printf("</P>\n");
  
}

#if 0
/* The function hasn't been used. #if 0 to get rid of compiler warning */
static int get_disk_usage(char *s)
{
  char cmd[8192];
  char buf[8192];
  FILE *fp;
  int i;

  sprintf(cmd,"du -s -k %s",s);
  if (flag_debug) printf("DEBUG  : du -s -k %s\n",s);
  fp = popen(cmd,"r");
  if (fp == NULL) {
    perror("du");
    return 0;
  }
  buf[0] = '\0';
  fgets(buf,8192,fp);
  fclose (fp);
  i = atoi(buf);
  return (i / 1024);
}
#endif

static void check_mem_size(int ro,char *rn)
{
  struct rlimit r;
  int rprev;
  long m_mb;
  int m_change_needed = 0;

  getrlimit(ro,&r);
  rprev = r.rlim_cur;
  r.rlim_cur = r.rlim_max;
  setrlimit(ro,&r);
  getrlimit(ro,&r);  

  if (flag_debug) printf("DEBUG  : %s (%d) max %d prev %d.\n",
			 rn, ro, (int)r.rlim_cur, rprev);

#if defined(__alpha) || defined(__ALPHA)
  if (r.rlim_cur <= 0L) return;
#endif  
  if (r.rlim_cur <= 0) return;
  
  m_mb = r.rlim_cur / 1048576;
  
  if (m_mb < mem_min) {   /* 64 MB */
    printf("ERROR  : processes are limited by %s to %ld MB in size.\n",
	   rn, m_mb);
    m_change_needed = 1;
    flag_os_bad = 1;
  } else if (m_mb <= mem_rec) {
    printf("WARNING: processes are limited by %s to %ld MB in size.\n",
	   rn, m_mb);
    m_change_needed = 1;
    flag_warnings++;
  }

  if (m_change_needed) {
#if defined(__hpux)
    printf("NOTICE : use kmtune or sam Kernel Configuration Parameters to change the maxdsiz\nand maxdsiz_64bit parameters.\n");
#endif
    printf("\n");
  }
}

static void limits_tests(void)
{
  struct rlimit r;

#if defined(RLIMIT_NOFILE)  
  getrlimit(RLIMIT_NOFILE,&r);

  if (r.rlim_max <= 1024) {
    if (flag_html) printf("<P>\n");

    if (flag_carrier) {
      printf("ERROR  : There are only %ld file descriptors (hard limit) available, which\nlimit the number of simultaneous connections.  ",r.rlim_max);
      flag_os_bad = 1;
    } else {
      printf("WARNING: There are only %ld file descriptors (hard limit) available, which\nlimit the number of simultaneous connections.  ",r.rlim_max);
      flag_warnings++;
    }

#if defined(__sun)
    printf("Additional file descriptors,\nup to 65536, are available by adding to /etc/system a line like\n");
    if (flag_html) printf("</P><PRE>\n");
    printf("set rlim_fd_max=4096\n");
    if (flag_html) printf("</PRE><P>\n");
#else
#if defined(__hpux)
	printf("Additional file descriptors,\nup to 60000, are available by editing /stand/system and regenerating the kernel.\n");
	if (flag_html) printf("</P><PRE>\n");
	printf("maxfiles_lim 4096\n");
	if (flag_html) printf("</PRE><P>\n");
#else
    printf("\n");
#endif
#endif
    printf("\n");

    if (flag_html) printf("</P>\n");

  } else {
    if (flag_debug) printf("DEBUG  : %ld descriptors (hard limit) available.\n",
			   r.rlim_max);
  }

  if (r.rlim_cur <= 1024) {
    if (flag_html) printf("<P>\n");

    if (flag_carrier) {
      printf("ERROR  : There are only %ld file descriptors (soft limit) available, which\nlimit the number of simultaneous connections.  ",r.rlim_cur);
      flag_os_bad = 1;
    } else {
      printf("WARNING: There are only %ld file descriptors (soft limit) available, which\nlimit the number of simultaneous connections.  ",r.rlim_cur);
      flag_warnings++;
    }

#if defined(__sun) || defined(__hpux)
    printf("Additional file descriptors,\nup to %ld (hard limit), are available by issuing \'ulimit\' (\'limit\' for tcsh)\ncommand with proper arguments.\n", r.rlim_max);
    if (flag_html) printf("</P><PRE>\n");
    printf("ulimit -n 4096\n");
    if (flag_html) printf("</PRE><P>\n");
#else
    printf("\n");
#endif
    printf("\n");

    if (flag_html) printf("</P>\n");

  } else {
    if (flag_debug) printf("DEBUG  : %ld descriptors (soft limit) available.\n",
			   r.rlim_cur);
  }
#endif

#if defined(RLIMIT_DATA)
  check_mem_size(RLIMIT_DATA,"RLIMIT_DATA");
#endif

#if defined(RLIMIT_VMEM)
  check_mem_size(RLIMIT_VMEM,"RLIMIT_VMEM");
#endif

#if defined(RLIMIT_AS)
  check_mem_size(RLIMIT_AS,"RLIMIT_AS");
#endif
}

/*
*** return the type of platform on which the software is running.
***/

static void ids_get_platform(char *buf)
{
#if defined(IDDS_LINUX_INCLUDE) || defined(__osf__) || defined(__hpux) || defined(IDDS_BSD_INCLUDE)
  	struct utsname u;
#endif
#if defined(__hpux)
	char model[128];
	char procstr[128];
	char oslevel[128];
#endif

#if defined(__hpux)
	long cpuvers, cputype;
#endif


#if defined(IDDS_LINUX_INCLUDE)
	if (uname(&u) == 0) {
	  sprintf(buf,"%s-unknown-linux%s",
		  u.machine,u.release);
	} else {
	  sprintf(buf,"i386-unknown-linux");
	}

#else
#if defined(__sun) 
	ids_get_platform_solaris(buf);
#elif defined(IDDS_BSD_INCLUDE)
	uname(&u);
	sprintf(buf,"%s-unknown-%s%s",
		u.machine,u.sysname,u.release);
#elif defined(__hpux)
	uname(&u);
	confstr(_CS_MACHINE_MODEL,model,128);
	cpuvers = sysconf(_SC_CPU_VERSION);
	cputype = sysconf(_SC_CPU_CHIP_TYPE);

	
	switch(cpuvers) {
	case CPU_PA_RISC1_0:
	  sprintf(procstr,"hppa1.0/%ld",cputype);
	  break;
	case CPU_PA_RISC1_1:
	  sprintf(procstr,"hppa1.1/%ld",cputype);
	  break;
	case CPU_PA_RISC1_2:
	  sprintf(procstr,"hppa1.2/%ld",cputype);
	  break;
	case CPU_PA_RISC2_0:
	  sprintf(procstr,"hppa2.0/%ld",cputype);
	  break;
#if defined(__ia64)
	case CPU_IA64_ARCHREV_0:
	  sprintf(procstr,"hpia0/%ld",cputype);
	  break;
#endif
	default:
	  sprintf(procstr,"hppa_0x%lx/%ld",cpuvers,cputype);
	  break;
	}

	sprintf(buf,"%s-hp%s-hpux_%s",procstr,model,u.release);


#else 
#if defined(__VMS)
#if defined (__ALPHA)
	sprintf(buf,"alpha-dec-vms");
#else
	sprintf(buf,"vax-dec-vms");
#endif

#else
#if defined(__osf__)
#if defined(__alpha) || defined(__ALPHA)
	ids_get_platform_tru64(buf);
#else
	sprintf(buf,"unknown-unknown-osf");
#endif
#else
#if defined(SI_HW_PROVIDER) && defined(SI_MACHINE) && defined(SI_SYSNAME) && defined(SI_RELEASE)
	if (1) {
	  char *bp;

	  sysinfo(SI_MACHINE,buf,64);
	  bp = buf + strlen(buf);
	  *bp = '-';
	  bp++;
	  sysinfo(SI_HW_PROVIDER,bp,64);
	  bp = bp + strlen(bp);
	  *bp = '-';
	  bp++;
	  sysinfo(SI_SYSNAME,bp,64);
	  bp = bp + strlen(bp);
	  sysinfo(SI_RELEASE,bp,64);
	}
#else   
#if defined(SI_MACHINE)
	sysinfo(SI_MACHINE,buf,64);
	strcat(buf,"-unknown-unknown");
#else
	sprintf(buf,"unknown");     
#endif /* has SI_HW_PROVIDER */
#endif /* has SI_MACHINE */
#endif /* OSF */
#endif /* VMS */
#endif /* HPUX */
#endif /* FREEBSD */
}

static int count_processors(void)
{
  int nproc = 0;

  nproc = sysconf(_SC_NPROCESSORS_ONLN);

  return nproc;
}

static void usage(char *av)
{
  printf("usage: %s [-q] [-D] [-v] [-c] [-i installdir]\n",av);
  printf("       -q dsktune only reports essential settings\n");
  printf("       -c dsktune only reports tuning information for client machines\n");
  printf("       -D dsktune also reports the commands executed\n");
  printf("       -v dsktune only reports its release version date\n");
  printf("       -i specify alternate server installation directory\n");
  printf("\n");
  exit(1);
}

static void print_version(void)
{
  printf("%s\n",build_date);
  exit(1);
}

int main(int argc,char *argv[])
{
  int i;

  while((i = getopt(argc,argv,"DvHqcCi:")) != EOF) {
    switch(i) {
    case 'D':
      flag_debug = 1;
      break;
    case 'H':
      flag_html = 1;
      break;
    case 'v':
      print_version();
      break;
    case 'q':
      flag_quick = 1;
      break;
    case 'c':
      client = 1;
      break;
    case 'C':
      flag_carrier = 1;
      break;
    case 'i':
      strncpy(install_dir,optarg,sizeof(install_dir));
      install_dir[sizeof(install_dir)-1] = '\0';
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (flag_quick == 0) {
    char sysbuf[BUFSIZ];
    int nproc;
    if (flag_html) printf("<P>\n");
    printf(CAPBRAND " Directory Server system tuning analysis version %s.\n\n", build_date);
    ids_get_platform(sysbuf);
    nproc = count_processors();
    if (nproc == 1) {
      printf("NOTICE : System is %s (1 processor).\n\n",sysbuf);
    } else if (nproc > 1) {
      printf("NOTICE : System is %s (%d processors).\n\n",sysbuf,nproc);
    } else {
      printf("NOTICE : System is %s.\n\n",sysbuf);
    }
    if (flag_html) printf("</P>\n");
  }

  gen_tests();

#if defined(__sun) || defined(__hpux) || defined(IDDS_LINUX_SYSCTL)
  ndd_tests();
#endif

#if defined(__hpux)
  hp_pthreads_tests();
#endif

  limits_tests();
  
  disk_tests();

  if (flag_os_bad || flag_arch_bad) {
    if (flag_html) printf("<P>\n");
    printf("ERROR  : The above errors MUST be corrected before proceeding.\n\n");
    if (flag_html) printf("</P>\n");
    exit(1);
  } else if (flag_warnings) {
    if (flag_html) printf("<P>\n");
    printf("WARNING  : The warning messages above should be reviewed before proceeding.\n\n");
    if (flag_html) printf("</P>\n");
    exit(1);
  }
  
  exit(0);
  return 0;
}

