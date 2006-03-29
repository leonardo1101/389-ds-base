#
# BEGIN COPYRIGHT BLOCK
# This Program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 2 of the License.
# 
# This Program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with
# this Program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA 02111-1307 USA.
# 
# In addition, as a special exception, Red Hat, Inc. gives You the additional
# right to link the code of this Program with code not covered under the GNU
# General Public License ("Non-GPL Code") and to distribute linked combinations
# including the two, subject to the limitations in this paragraph. Non-GPL Code
# permitted under this exception must only link to the code of this Program
# through those well defined interfaces identified in the file named EXCEPTION
# found in the source code files (the "Approved Interfaces"). The files of
# Non-GPL Code may instantiate templates or use macros or inline functions from
# the Approved Interfaces without causing the resulting work to be covered by
# the GNU General Public License. Only Red Hat, Inc. may make changes or
# additions to the list of Approved Interfaces. You must obey the GNU General
# Public License in all respects for all of the Program code and other code used
# in conjunction with the Program except the Non-GPL Code covered by this
# exception. If you modify this file, you may extend this exception to your
# version of the file, but you are not obligated to do so. If you do not wish to
# provide this exception without modification, you must delete this exception
# statement from your version and license this file solely under the GPL without
# exception. 
# 
# 
# Copyright (C) 2005 Red Hat, Inc.
# All rights reserved.
# END COPYRIGHT BLOCK
#
# This file is where you tell the build process where to find the
# various components used during the build process.

# You can either use components built locally from source or
# pre-built components.  The reason for the different macros
# for SOURCE and BUILD is that the locations for the libs, includes,
# etc. are usually different for packages built from source vs.
# pre-built packages.  As an example, when building NSPR from
# source, the includes are in mozilla/dist/$(OBJDIR_NAME)/include
# where OBJDIR_NAME includes the OS, arch, compiler, thread model, etc.
# When using the pre-built NSPR from Mozilla FTP, the include files
# are just in nsprdir/include.  This is why we have to make the
# distinction between a SOURCE component and a BUILD (pre-built)
# component.  See components.mk for the gory details.

# For each component, specify the source root OR the pre-built
# component directory.  If both a SOURCE_ROOT and a BUILD_DIR are
# defined for a component, the SOURCE_ROOT will be used - don't do
# this, it's confusing.

# For the Mozilla components, if using source for all of them,
# you can just define MOZILLA_SOURCE_ROOT - the build will
# assume all of them have been built in that same directory
# (as per the recommended build instructions)

# For all components, the recommended way is to put each
# component in a subdirectory of the parent directory of
# BUILD_ROOT, both with pre-built and source components

MOZILLA_SOURCE_ROOT = $(BUILD_ROOT)/../mozilla
ifdef MOZILLA_SOURCE_ROOT
  ifndef MOZ_OBJDIR_NAME
    # some of the mozilla components are put in a platform/buildtype specific
    # subdir of mozilla/dist, and their naming convention is different than
    # ours - we need to map ours to theirs
    ifneq (,$(findstring RHEL3,$(NSOBJDIR_NAME)))
      MOZ_OBJDIR_NAME = $(subst _gcc3_,_glibc_PTH$(NS64TAG)_,$(subst RHEL3,Linux2.4,$(NSOBJDIR_NAME)))
    else
    ifneq (,$(findstring RHEL4,$(NSOBJDIR_NAME)))
      MOZ_OBJDIR_NAME = $(subst _gcc3_,_glibc_PTH$(NS64TAG)_,$(subst RHEL4,Linux2.6,$(NSOBJDIR_NAME)))
    else
    ifneq (,$(findstring Linux,$(NSOBJDIR_NAME)))
      MOZ_OBJDIR_NAME = $(subst _glibc_PTH_,_glibc_PTH$(NS64TAG)_,$(NSOBJDIR_NAME))
    else
      MOZ_OBJDIR_NAME = $(NSOBJDIR_NAME)
    endif
    endif
    endif
  endif
endif

NSPR_SOURCE_ROOT = $(MOZILLA_SOURCE_ROOT)
#NSPR_BUILD_DIR = $(BUILD_ROOT)/../nspr-4.4.1
# NSPR also needs a build dir with a full, absolute path for some reason
#NSPR_ABS_BUILD_DIR = $(shell cd $(NSPR_BUILD_DIR) && pwd)

DBM_SOURCE_ROOT = $(MOZILLA_SOURCE_ROOT)
#DBM_BUILD_DIR = $(BUILD_ROOT)/../nss-3.9.3

SECURITY_SOURCE_ROOT = $(MOZILLA_SOURCE_ROOT)
#SECURITY_BUILD_DIR = $(BUILD_ROOT)/../nss-3.9.3

SVRCORE_SOURCE_ROOT = $(MOZILLA_SOURCE_ROOT)
#SVRCORE_BUILD_DIR = $(BUILD_ROOT)/../svrcore-4.0

LDAPSDK_SOURCE_ROOT = $(MOZILLA_SOURCE_ROOT)
#LDAP_ROOT = $(BUILD_ROOT)/../ldapsdk-5.15

SASL_SOURCE_ROOT = $(BUILD_ROOT)/../cyrus-sasl-2.1.20/built
#SASL_BUILD_DIR = $(BUILD_ROOT)/../sasl

ICU_SOURCE_ROOT = $(BUILD_ROOT)/../icu
#ICU_BUILD_DIR = $(BUILD_ROOT)/../icu-2.4

DB_SOURCE_ROOT = $(BUILD_ROOT)/../db-4.2.52.NC
# DB_MAJOR_MINOR is the root name for the db shared library
# source builds use db-4.2 - lib is prepended later
DB_MAJOR_MINOR := db-4.2
# internal builds rename this to be db42
#DB_MAJOR_MINOR := db42
#component_name:=$(DB_MAJOR_MINOR)
#db_path_config:=$(BUILD_ROOT)/../$(db_component_name)

NETSNMP_SOURCE_ROOT = $(BUILD_ROOT)/../net-snmp-5.2.1
#NETSNMP_BUILD_DIR = $(BUILD_ROOT)/../net-snmp

ADMINUTIL_SOURCE_ROOT = $(BUILD_ROOT)/../adminutil
#ADMINUTIL_BUILD_DIR = $(NSCP_DISTDIR_FULL_RTL)/adminutil

SETUPUTIL_SOURCE_ROOT = $(BUILD_ROOT)/../setuputil
#SETUPUTIL_BUILD_DIR = $(NSCP_DISTDIR_FULL_RTL)/setuputil

# it's customary and easier to use the pre-built jar
# you can get this from www.jpackage.org as well
# we usually get this from the admin server package which
# gets it from the console package
#LDAPJDK_SOURCE_DIR = $(MOZILLA_SOURCE_ROOT)
# Crimson - crimson.jar - http://xml.apache.org/crimson/
# you can get this from www.jpackage.org as well
#CRIMSON_SOURCE_DIR = $(BUILD_ROOT)/../crimson

ADMINSERVER_SOURCE_ROOT = $(BUILD_ROOT)/../adminserver

LDAPCONSOLE_SOURCE_ROOT = $(BUILD_ROOT)/../directoryconsole

# these are the files needed to build the java components - xmltools and dsmlgw -
# and where to get them
# NOTE: www.jpackage.org has almost all of these, and you can configure yum or apt
# or up2date or whatever your package manager is to pull them, which is nice because it will
# also take care of the dependencies - http://www.jpackage.org/repos.php
# Axis - axis.jar - http://ws.apache.org/axis/index.html - also jaxrpc.jar,saaj.jar
# Xerces-J - xercesImpl.jar, xml-apis.jar http://xml.apache.org/xerces2-j/download.cgi
# JAF - activation.jar - http://java.sun.com/products/javabeans/glasgow/jaf.html
# OR
# JAF - activation.jar or jaf.jar - http://www.jpackage.org
# NOTE - classpathx-jaf may also work
# Codec - jakarta-commons-codec.jar - http://jakarta.apache.org/commons/codec/
# JWSDP - jaxrpc-api.jar,jaxrpc.jar,saaj.jar - http://java.sun.com/webservices/downloads/webservicespack.html
# all of these files need to be in the following directory:
DSMLGWJARS_BUILD_DIR = $(BUILD_ROOT)/../dsmlgwjars

PERLDAP_SOURCE_ROOT = $(MOZILLA_SOURCE_ROOT)

ONLINEHELP_SOURCE_ROOT = $(BUILD_ROOT)/../dsonlinehelp
