# --- BEGIN COPYRIGHT BLOCK ---
# Copyright (C) 2015 Red Hat, Inc.
# All rights reserved.
#
# License: GPL (version 3 or any later version).
# See LICENSE for details.
# --- END COPYRIGHT BLOCK ---
#
import os
import sys
import time
import ldap
import logging
import pytest
from lib389 import DirSrv, Entry, tools, tasks
from lib389.tools import DirSrvTools
from lib389._constants import *
from lib389.properties import *
from lib389.tasks import *
from lib389.utils import *

log = logging.getLogger(__name__)

installation_prefix = None

# Assuming DEFAULT_SUFFIX is "dc=example,dc=com", otherwise it does not work... :(
SUBTREE_CONTAINER = 'cn=nsPwPolicyContainer,' + DEFAULT_SUFFIX
SUBTREE_PWPDN = 'cn=nsPwPolicyEntry,' + DEFAULT_SUFFIX
SUBTREE_PWP = 'cn=cn\3DnsPwPolicyEntry\2Cdc\3Dexample\2Cdc\3Dcom,' + SUBTREE_CONTAINER
SUBTREE_COS_TMPLDN = 'cn=nsPwTemplateEntry,' + DEFAULT_SUFFIX
SUBTREE_COS_TMPL = 'cn=cn\3DnsPwTemplateEntry\2Cdc\3Dexample\2Cdc\3Dcom,' + SUBTREE_CONTAINER
SUBTREE_COS_DEF = 'cn=nsPwPolicy_CoS,' + DEFAULT_SUFFIX

USER1_DN = 'uid=user1,' + DEFAULT_SUFFIX
USER2_DN = 'uid=user2,' + DEFAULT_SUFFIX
USER3_DN = 'uid=user3,' + DEFAULT_SUFFIX
USER_PW = 'password'

class TopologyStandalone(object):
    def __init__(self, standalone):
        standalone.open()
        self.standalone = standalone


@pytest.fixture(scope="module")
def topology(request):
    global installation_prefix
    if installation_prefix:
        args_instance[SER_DEPLOYED_DIR] = installation_prefix

    # Creating standalone instance ...
    standalone = DirSrv(verbose=False)
    args_instance[SER_HOST] = HOST_STANDALONE
    args_instance[SER_PORT] = PORT_STANDALONE
    args_instance[SER_SERVERID_PROP] = SERVERID_STANDALONE
    args_instance[SER_CREATION_SUFFIX] = DEFAULT_SUFFIX
    args_standalone = args_instance.copy()
    standalone.allocate(args_standalone)
    instance_standalone = standalone.exists()
    if instance_standalone:
        standalone.delete()
    standalone.create()
    standalone.open()

    # Delete each instance in the end
    def fin():
        standalone.delete()
    request.addfinalizer(fin)

    # Clear out the tmp dir
    standalone.clearTmpDir(__file__)

    return TopologyStandalone(standalone)


def set_global_pwpolicy(topology):
    log.info("	+++++ Enable global password policy +++++\n")
    # Enable password policy
    try:
        topology.standalone.modify_s(DN_CONFIG, [(ldap.MOD_REPLACE, 'nsslapd-pwpolicy-local', 'on')])
    except ldap.LDAPError as e:
        log.error('Failed to set pwpolicy-local: error ' + e.message['desc'])
        assert False

    log.info("		Set global password Min Age -- 1 day\n")
    try:
        topology.standalone.modify_s(DN_CONFIG, [(ldap.MOD_REPLACE, 'passwordMinAge', '86400')])
    except ldap.LDAPError as e:
        log.error('Failed to set passwordMinAge: error ' + e.message['desc'])
        assert False

    log.info("		Set global password Max Age -- 10 days\n")
    try:
        topology.standalone.modify_s(DN_CONFIG, [(ldap.MOD_REPLACE, 'passwordMaxAge', '864000')])
    except ldap.LDAPError as e:
        log.error('Failed to set passwordMaxAge: error ' + e.message['desc'])
        assert False

    log.info("		Set global password Warning -- 3 days\n")
    try:
        topology.standalone.modify_s(DN_CONFIG, [(ldap.MOD_REPLACE, 'passwordWarning', '259200')])
    except ldap.LDAPError as e:
        log.error('Failed to set passwordWarning: error ' + e.message['desc'])
        assert False


def set_subtree_pwpolicy(topology):
    log.info("	+++++ Enable subtree level password policy +++++\n")
    log.info("		Add the container")
    try:
        topology.standalone.add_s(Entry((SUBTREE_CONTAINER, {'objectclass': 'top nsContainer'.split(),
                                                             'cn': 'nsPwPolicyContainer'})))
    except ldap.LDAPError as e:
        log.error('Failed to add subtree container: error ' + e.message['desc'])
        assert False

    log.info("		Add the password policy subentry {passwordMustChange: on, passwordMinAge: 2, passwordMaxAge: 20, passwordWarning: 6}")
    try:
        topology.standalone.add_s(Entry((SUBTREE_PWP, {'objectclass': 'top ldapsubentry passwordpolicy'.split(),
                                                       'cn': SUBTREE_PWPDN,
                                                       'passwordMustChange': 'on',
                                                       'passwordExp': 'on',
                                                       'passwordMinAge': '172800',
                                                       'passwordMaxAge': '1728000',
                                                       'passwordWarning': '518400',
                                                       'passwordChange': 'on',
                                                       'passwordStorageScheme': 'clear'})))
    except ldap.LDAPError as e:
        log.error('Failed to add passwordpolicy: error ' + e.message['desc'])
        assert False

    log.info("		Add the COS template")
    try:
        topology.standalone.add_s(Entry((SUBTREE_COS_TMPL, {'objectclass': 'top ldapsubentry costemplate extensibleObject'.split(),
                                                            'cn': SUBTREE_PWPDN,
                                                            'cosPriority': '1',
                                                            'cn': SUBTREE_COS_TMPLDN,
                                                            'pwdpolicysubentry': SUBTREE_PWP})))
    except ldap.LDAPError as e:
        log.error('Failed to add COS template: error ' + e.message['desc'])
        assert False

    log.info("		Add the COS definition")
    try:
        topology.standalone.add_s(Entry((SUBTREE_COS_DEF, {'objectclass': 'top ldapsubentry cosSuperDefinition cosPointerDefinition'.split(),
                                                           'cn': SUBTREE_PWPDN,
                                                           'costemplatedn': SUBTREE_COS_TMPL,
                                                           'cosAttribute': 'pwdpolicysubentry default operational-default'})))
    except ldap.LDAPError as e:
        log.error('Failed to add COS def: error ' + e.message['desc'])
        assert False

    time.sleep(1)


def update_passwd(topology, user, passwd, newpasswd):
    log.info("		Bind as {%s,%s}" % (user, passwd))
    topology.standalone.simple_bind_s(user, passwd)
    try:
        topology.standalone.modify_s(user, [(ldap.MOD_REPLACE, 'userpassword', newpasswd)])
    except ldap.LDAPError as e:
        log.fatal('test_ticket548: Failed to update the password ' + cpw + ' of user ' + user + ': error ' + e.message['desc'])
        assert False

    time.sleep(1)


def check_shadow_attr_value(entry, attr_type, expected, dn):
    if entry.hasAttr(attr_type):
        actual = entry.getValue(attr_type)
        if int(actual) == expected:
            log.info('%s of entry %s has expected value %s' % (attr_type, dn, actual))
            assert True
        else:
            log.fatal('%s %s of entry %s does not have expected value %s' % (attr_type, actual, dn, expected))
            assert False
    else:
        log.fatal('entry %s does not have %s attr' % (dn, attr_type))
        assert False


def test_ticket548_test_with_no_policy(topology):
    """
    Check shadowAccount under no password policy
    """
    log.info("Case 1. No password policy")

    log.info("Bind as %s" % DN_DM)
    topology.standalone.simple_bind_s(DN_DM, PASSWORD)

    log.info('Add an entry' + USER1_DN)
    try:
        topology.standalone.add_s(Entry((USER1_DN, {'objectclass': "top person organizationalPerson inetOrgPerson shadowAccount".split(),
                                     'sn': '1',
                                     'cn': 'user 1',
                                     'uid': 'user1',
                                     'givenname': 'user',
                                     'mail': 'user1@example.com',
                                     'userpassword': USER_PW})))
    except ldap.LDAPError as e:
        log.fatal('test_ticket548: Failed to add user' + USER1_DN + ': error ' + e.message['desc'])
        assert False

    edate = int(time.time() / (60 * 60 * 24))
    log.info('Search entry %s' % USER1_DN)

    log.info("Bind as %s" % USER1_DN)
    topology.standalone.simple_bind_s(USER1_DN, USER_PW)
    entry = topology.standalone.getEntry(USER1_DN, ldap.SCOPE_BASE, "(objectclass=*)", ['shadowLastChange'])
    check_shadow_attr_value(entry, 'shadowLastChange', edate, USER1_DN)

    log.info("Check shadowAccount with no policy was successfully verified.")


def test_ticket548_test_global_policy(topology):
    """
    Check shadowAccount with global password policy
    """

    log.info("Case 2.  Check shadowAccount with global password policy")

    log.info("Bind as %s" % DN_DM)
    topology.standalone.simple_bind_s(DN_DM, PASSWORD)

    set_global_pwpolicy(topology)

    log.info('Add an entry' + USER2_DN)
    try:
        topology.standalone.add_s(Entry((USER2_DN, {'objectclass': "top person organizationalPerson inetOrgPerson shadowAccount".split(),
                                     'sn': '2',
                                     'cn': 'user 2',
                                     'uid': 'user2',
                                     'givenname': 'user',
                                     'mail': 'user2@example.com',
                                     'userpassword': USER_PW})))
    except ldap.LDAPError as e:
        log.fatal('test_ticket548: Failed to add user' + USER2_DN + ': error ' + e.message['desc'])
        assert False

    log.info("Bind as %s" % USER2_DN)
    topology.standalone.simple_bind_s(USER2_DN, USER_PW)

    edate = int(time.time() / (60 * 60 * 24))
    log.info('Search entry %s' % USER2_DN)
    entry = topology.standalone.getEntry(USER2_DN, ldap.SCOPE_BASE, "(objectclass=*)")
    check_shadow_attr_value(entry, 'shadowLastChange', edate, USER2_DN)

    # passwordMinAge -- 1 day
    check_shadow_attr_value(entry, 'shadowMin', 1, USER2_DN)

    # passwordMaxAge -- 10 days
    check_shadow_attr_value(entry, 'shadowMax', 10, USER2_DN)

    # passwordWarning -- 3 days
    check_shadow_attr_value(entry, 'shadowWarning', 3, USER2_DN)

    log.info("Check shadowAccount with global policy was successfully verified.")


def test_ticket548_test_subtree_policy(topology):
    """
    Check shadowAccount with subtree level password policy
    """

    log.info("Case 3.  Check shadowAccount with subtree level password policy")

    log.info("Bind as %s" % DN_DM)
    topology.standalone.simple_bind_s(DN_DM, PASSWORD)

    set_subtree_pwpolicy(topology)

    log.info('Add an entry' + USER3_DN)
    try:
        topology.standalone.add_s(Entry((USER3_DN, {'objectclass': "top person organizationalPerson inetOrgPerson shadowAccount".split(),
                                     'sn': '3',
                                     'cn': 'user 3',
                                     'uid': 'user3',
                                     'givenname': 'user',
                                     'mail': 'user3@example.com',
                                     'userpassword': USER_PW})))
    except ldap.LDAPError as e:
        log.fatal('test_ticket548: Failed to add user' + USER3_DN + ': error ' + e.message['desc'])
        assert False

    log.info('Search entry %s' % USER3_DN)
    entry0 = topology.standalone.getEntry(USER3_DN, ldap.SCOPE_BASE, "(objectclass=*)")

    log.info('Expecting shadowLastChange 0 since passwordMustChange is on')
    check_shadow_attr_value(entry0, 'shadowLastChange', 0, USER3_DN)

    # passwordMinAge -- 2 day
    check_shadow_attr_value(entry0, 'shadowMin', 2, USER3_DN)

    # passwordMaxAge -- 20 days
    check_shadow_attr_value(entry0, 'shadowMax', 20, USER3_DN)

    # passwordWarning -- 6 days
    check_shadow_attr_value(entry0, 'shadowWarning', 6, USER3_DN)

    log.info("Bind as %s" % USER3_DN)
    topology.standalone.simple_bind_s(USER3_DN, USER_PW)

    log.info('Search entry %s' % USER3_DN)
    try:
        entry1 = topology.standalone.getEntry(USER3_DN, ldap.SCOPE_BASE, "(objectclass=*)")
    except ldap.UNWILLING_TO_PERFORM:
        log.info('test_ticket548: Search by' + USER3_DN + ' failed by UNWILLING_TO_PERFORM as expected')
    except ldap.LDAPError as e:
        log.fatal('test_ticket548: Failed to serch user' + USER3_DN + ' by self: error ' + e.message['desc'])
        assert False

    log.info("Bind as %s and updating the password with a new one" % USER3_DN)
    topology.standalone.simple_bind_s(USER3_DN, USER_PW)

    newpasswd = USER_PW + '0'
    update_passwd(topology, USER3_DN, USER_PW, newpasswd)

    log.info("Re-bind as %s with new password" % USER3_DN)
    topology.standalone.simple_bind_s(USER3_DN, newpasswd)

    try:
        entry2 = topology.standalone.getEntry(USER3_DN, ldap.SCOPE_BASE, "(objectclass=*)")
    except ldap.LDAPError as e:
        log.fatal('test_ticket548: Failed to serch user' + USER3_DN + ' by self: error ' + e.message['desc'])
        assert False

    edate = int(time.time() / (60 * 60 * 24))

    log.info('Expecting shadowLastChange %d once userPassword is updated', edate)
    check_shadow_attr_value(entry2, 'shadowLastChange', edate, USER3_DN)

    log.info("Check shadowAccount with subtree level policy was successfully verified.")


if __name__ == '__main__':
    # Run isolated
    # -s for DEBUG mode
    CURRENT_FILE = os.path.realpath(__file__)
    pytest.main("-s %s" % CURRENT_FILE)  
