# --- BEGIN COPYRIGHT BLOCK ---
# Copyright (C) 2015 Red Hat, Inc.
# All rights reserved.
#
# License: GPL (version 3 or any later version).
# See LICENSE for details. 
# --- END COPYRIGHT BLOCK ---
#
'''
Created on April 14, 2014

@author: tbordaz
'''
import os
import sys
import time
import ldap
import logging
import pytest
import re
from lib389 import DirSrv, Entry, tools, NoSuchEntryError
from lib389.tools import DirSrvTools
from lib389._constants import *
from lib389.properties import *
from lib389._constants import REPLICAROLE_MASTER

logging.getLogger(__name__).setLevel(logging.DEBUG)
log = logging.getLogger(__name__)

#
# important part. We can deploy Master1 and Master2 on different versions
#
installation1_prefix = None
installation2_prefix = None

# set this flag to False so that it will assert on failure _status_entry_both_server
DEBUG_FLAG = False

TEST_REPL_DN = "cn=test_repl, %s" % SUFFIX

STAGING_CN     = "staged user"
PRODUCTION_CN  = "accounts"
EXCEPT_CN      = "excepts"

STAGING_DN    = "cn=%s,%s" % (STAGING_CN, SUFFIX)
PRODUCTION_DN = "cn=%s,%s" % (PRODUCTION_CN, SUFFIX)
PROD_EXCEPT_DN = "cn=%s,%s" % (EXCEPT_CN, PRODUCTION_DN)

STAGING_PATTERN    = "cn=%s*,%s" % (STAGING_CN[:2],    SUFFIX)
PRODUCTION_PATTERN = "cn=%s*,%s" % (PRODUCTION_CN[:2], SUFFIX)
BAD_STAGING_PATTERN    = "cn=bad*,%s" % (SUFFIX)
BAD_PRODUCTION_PATTERN = "cn=bad*,%s" % (SUFFIX)

BIND_CN        = "bind_entry"
BIND_DN        = "cn=%s,%s" % (BIND_CN, SUFFIX)
BIND_PW        = "password"

NEW_ACCOUNT    = "new_account"
MAX_ACCOUNTS   = 20

CONFIG_MODDN_ACI_ATTR = "nsslapd-moddn-aci"


class TopologyMaster1Master2(object):
    def __init__(self, master1, master2):
        master1.open()
        self.master1 = master1

        master2.open()
        self.master2 = master2


@pytest.fixture(scope="module")
def topology(request):
    '''
        This fixture is used to create a replicated topology for the 'module'.
        The replicated topology is MASTER1 <-> Master2.
    '''
    global installation1_prefix
    global installation2_prefix

    # allocate master1 on a given deployement
    master1 = DirSrv(verbose=False)
    if installation1_prefix:
        args_instance[SER_DEPLOYED_DIR] = installation1_prefix

    # Args for the master1 instance
    args_instance[SER_HOST] = HOST_MASTER_1
    args_instance[SER_PORT] = PORT_MASTER_1
    args_instance[SER_SERVERID_PROP] = SERVERID_MASTER_1
    args_master = args_instance.copy()
    master1.allocate(args_master)

    # allocate master1 on a given deployement
    master2 = DirSrv(verbose=False)
    if installation2_prefix:
        args_instance[SER_DEPLOYED_DIR] = installation2_prefix

    # Args for the consumer instance
    args_instance[SER_HOST] = HOST_MASTER_2
    args_instance[SER_PORT] = PORT_MASTER_2
    args_instance[SER_SERVERID_PROP] = SERVERID_MASTER_2
    args_master = args_instance.copy()
    master2.allocate(args_master)

    # Get the status of the instance and restart it if it exists
    instance_master1 = master1.exists()
    instance_master2 = master2.exists()

    # Remove all the instances
    if instance_master1:
        master1.delete()
    if instance_master2:
        master2.delete()

    # Create the instances
    master1.create()
    master1.open()
    master2.create()
    master2.open()

    #
    # Now prepare the Master-Consumer topology
    #
    # First Enable replication
    master1.replica.enableReplication(suffix=SUFFIX, role=REPLICAROLE_MASTER, replicaId=REPLICAID_MASTER_1)
    master2.replica.enableReplication(suffix=SUFFIX, role=REPLICAROLE_MASTER, replicaId=REPLICAID_MASTER_2)

    # Initialize the supplier->consumer

    properties = {RA_NAME:      r'meTo_$host:$port',
                  RA_BINDDN:    defaultProperties[REPLICATION_BIND_DN],
                  RA_BINDPW:    defaultProperties[REPLICATION_BIND_PW],
                  RA_METHOD:    defaultProperties[REPLICATION_BIND_METHOD],
                  RA_TRANSPORT_PROT: defaultProperties[REPLICATION_TRANSPORT]}
    repl_agreement = master1.agreement.create(suffix=SUFFIX, host=master2.host, port=master2.port, properties=properties)

    if not repl_agreement:
        log.fatal("Fail to create a replica agreement")
        sys.exit(1)

    log.debug("%s created" % repl_agreement)

    properties = {RA_NAME:      r'meTo_$host:$port',
                  RA_BINDDN:    defaultProperties[REPLICATION_BIND_DN],
                  RA_BINDPW:    defaultProperties[REPLICATION_BIND_PW],
                  RA_METHOD:    defaultProperties[REPLICATION_BIND_METHOD],
                  RA_TRANSPORT_PROT: defaultProperties[REPLICATION_TRANSPORT]}
    master2.agreement.create(suffix=SUFFIX, host=master1.host, port=master1.port, properties=properties)

    master1.agreement.init(SUFFIX, HOST_MASTER_2, PORT_MASTER_2)
    master1.waitForReplInit(repl_agreement)

    # Check replication is working fine
    if master1.testReplication(DEFAULT_SUFFIX, master2):
        log.info('Replication is working.')
    else:
        log.fatal('Replication is not working.')
        assert False

    # clear the tmp directory
    master1.clearTmpDir(__file__)

    # Here we have two instances master and consumer
    # with replication working.
    return TopologyMaster1Master2(master1, master2)


def _bind_manager(server):
    server.log.info("Bind as %s " % DN_DM)
    server.simple_bind_s(DN_DM, PASSWORD)


def _bind_normal(server):
    server.log.info("Bind as %s " % BIND_DN)
    server.simple_bind_s(BIND_DN, BIND_PW)


def _header(topology, label):
    topology.master1.log.info("\n\n###############################################")
    topology.master1.log.info("#######")
    topology.master1.log.info("####### %s" % label)
    topology.master1.log.info("#######")
    topology.master1.log.info("###############################################")


def _status_entry_both_server(topology, name=None, desc=None, debug=True):
    if not name:
        return
    topology.master1.log.info("\n\n######################### Tombstone on M1 ######################\n")
    attr = 'description'
    found = False
    attempt = 0
    while not found and attempt < 10:
        ent_m1 = _find_tombstone(topology.master1, SUFFIX, 'sn', name)
        if attr in ent_m1.getAttrs():
            found = True
        else:
            time.sleep(1)
            attempt = attempt + 1
    assert ent_m1

    topology.master1.log.info("\n\n######################### Tombstone on M2 ######################\n")
    ent_m2 = _find_tombstone(topology.master2, SUFFIX, 'sn', name)
    assert ent_m2

    topology.master1.log.info("\n\n######################### Description ######################\n%s\n" % desc)
    topology.master1.log.info("M1 only\n")
    for attr in ent_m1.getAttrs():

        if not debug:
            assert attr in ent_m2.getAttrs()

        if not attr in ent_m2.getAttrs():
            topology.master1.log.info("    %s" % attr)
            for val in ent_m1.getValues(attr):
                topology.master1.log.info("        %s" % val)

    topology.master1.log.info("M2 only\n")
    for attr in ent_m2.getAttrs():

        if not debug:
            assert attr in ent_m1.getAttrs()

        if not attr in ent_m1.getAttrs():
            topology.master1.log.info("    %s" % attr)
            for val in ent_m2.getValues(attr):
                topology.master1.log.info("        %s" % val)

    topology.master1.log.info("M1 differs M2\n")

    if not debug:
        assert ent_m1.dn == ent_m2.dn

    if ent_m1.dn != ent_m2.dn:
        topology.master1.log.info("    M1[dn] = %s\n    M2[dn] = %s" % (ent_m1.dn, ent_m2.dn))

    for attr1 in ent_m1.getAttrs():
        if attr1 in ent_m2.getAttrs():
            for val1 in ent_m1.getValues(attr1):
                found = False
                for val2 in ent_m2.getValues(attr1):
                    if val1 == val2:
                        found = True
                        break

                if not debug:
                    assert found

                if not found:
                    topology.master1.log.info("    M1[%s] = %s" % (attr1, val1))

    for attr2 in ent_m2.getAttrs():
        if attr2 in ent_m1.getAttrs():
            for val2 in ent_m2.getValues(attr2):
                found = False
                for val1 in ent_m1.getValues(attr2):
                    if val2 == val1:
                        found = True
                        break

                if not debug:
                    assert found

                if not found:
                    topology.master1.log.info("    M2[%s] = %s" % (attr2, val2))


def _pause_RAs(topology):
    topology.master1.log.info("\n\n######################### Pause RA M1<->M2 ######################\n")
    ents = topology.master1.agreement.list(suffix=SUFFIX)
    assert len(ents) == 1
    topology.master1.agreement.pause(ents[0].dn)

    ents = topology.master2.agreement.list(suffix=SUFFIX)
    assert len(ents) == 1
    topology.master2.agreement.pause(ents[0].dn)


def _resume_RAs(topology):
    topology.master1.log.info("\n\n######################### resume RA M1<->M2 ######################\n")
    ents = topology.master1.agreement.list(suffix=SUFFIX)
    assert len(ents) == 1
    topology.master1.agreement.resume(ents[0].dn)

    ents = topology.master2.agreement.list(suffix=SUFFIX)
    assert len(ents) == 1
    topology.master2.agreement.resume(ents[0].dn)


def _find_tombstone(instance, base, attr, value):
    #
    # we can not use a filter with a (&(objeclass=nsTombstone)(sn=name)) because
    # tombstone are not index in 'sn' so 'sn=name' will return NULL
    # and even if tombstone are indexed for objectclass the '&' will set
    # the candidate list to NULL
    #
    filt = '(objectclass=%s)' % REPLICA_OC_TOMBSTONE
    ents = instance.search_s(base, ldap.SCOPE_SUBTREE, filt)
    #found = False
    for ent in ents:
        if ent.hasAttr(attr):
            for val in ent.getValues(attr):
                if val == value:
                    instance.log.debug("tombstone found: %r" % ent)
                    return ent
    return None


def _delete_entry(instance, entry_dn, name):
    instance.log.info("\n\n######################### DELETE %s (M1) ######################\n" % name)

    # delete the entry
    instance.delete_s(entry_dn)
    assert _find_tombstone(instance, SUFFIX, 'sn', name) is not None


def _mod_entry(instance, entry_dn, attr, value):
    instance.log.info("\n\n######################### MOD %s (M2) ######################\n" % entry_dn)
    mod = [(ldap.MOD_REPLACE, attr, value)]
    instance.modify_s(entry_dn, mod)


def _modrdn_entry(instance=None, entry_dn=None, new_rdn=None, del_old=0, new_superior=None):
    assert instance is not None
    assert entry_dn is not None

    if not new_rdn:
        pattern = 'cn=(.*),(.*)'
        rdnre = re.compile(pattern)
        match = rdnre.match(entry_dn)
        old_value = match.group(1)
        new_rdn_val = "%s_modrdn" % old_value
        new_rdn = "cn=%s" % new_rdn_val

    instance.log.info("\n\n######################### MODRDN %s (M2) ######################\n" % new_rdn)
    if new_superior:
        instance.rename_s(entry_dn, new_rdn, newsuperior=new_superior, delold=del_old)
    else:
        instance.rename_s(entry_dn, new_rdn, delold=del_old)


def _check_entry_exists(instance, entry_dn):
    loop = 0
    ent = None
    while loop <= 10:
        try:
            ent = instance.getEntry(entry_dn, ldap.SCOPE_BASE, "(objectclass=*)")
            break
        except ldap.NO_SUCH_OBJECT:
            time.sleep(1)
            loop += 1
    if ent is None:
        assert False


def _check_mod_received(instance, base, filt, attr, value):
    instance.log.info("\n\n######################### Check MOD replicated on %s ######################\n" % instance.serverid)
    loop = 0
    while loop <= 10:
        ent = instance.getEntry(base, ldap.SCOPE_SUBTREE, filt)
        if ent.hasAttr(attr) and ent.getValue(attr) == value:
                break
        time.sleep(1)
        loop += 1
    assert loop <= 10


def _check_replication(topology, entry_dn):
    # prepare the filter to retrieve the entry
    filt = entry_dn.split(',')[0]

    topology.master1.log.info("\n######################### Check replicat M1->M2 ######################\n")
    loop = 0
    while loop <= 10:
        attr = 'description'
        value = 'test_value_%d' % loop
        mod = [(ldap.MOD_REPLACE, attr, value)]
        topology.master1.modify_s(entry_dn, mod)
        _check_mod_received(topology.master2, SUFFIX, filt, attr, value)
        loop += 1

    topology.master1.log.info("\n######################### Check replicat M2->M1 ######################\n")
    loop = 0
    while loop <= 10:
        attr = 'description'
        value = 'test_value_%d' % loop
        mod = [(ldap.MOD_REPLACE, attr, value)]
        topology.master2.modify_s(entry_dn, mod)
        _check_mod_received(topology.master1, SUFFIX, filt, attr, value)
        loop += 1


def test_ticket47787_init(topology):
    """
        Creates
            - a staging DIT
            - a production DIT
            - add accounts in staging DIT

    """

    topology.master1.log.info("\n\n######################### INITIALIZATION ######################\n")

    # entry used to bind with
    topology.master1.log.info("Add %s" % BIND_DN)
    topology.master1.add_s(Entry((BIND_DN, {
                                            'objectclass': "top person".split(),
                                            'sn':           BIND_CN,
                                            'cn':           BIND_CN,
                                            'userpassword': BIND_PW})))

    # DIT for staging
    topology.master1.log.info("Add %s" % STAGING_DN)
    topology.master1.add_s(Entry((STAGING_DN, {
                                            'objectclass': "top organizationalRole".split(),
                                            'cn':           STAGING_CN,
                                            'description': "staging DIT"})))

    # DIT for production
    topology.master1.log.info("Add %s" % PRODUCTION_DN)
    topology.master1.add_s(Entry((PRODUCTION_DN, {
                                            'objectclass': "top organizationalRole".split(),
                                            'cn':           PRODUCTION_CN,
                                            'description': "production DIT"})))

    # enable replication error logging
    mod = [(ldap.MOD_REPLACE, 'nsslapd-errorlog-level', '8192')]
    topology.master1.modify_s(DN_CONFIG, mod)
    topology.master2.modify_s(DN_CONFIG, mod)

    # add dummy entries in the staging DIT
    for cpt in range(MAX_ACCOUNTS):
        name = "%s%d" % (NEW_ACCOUNT, cpt)
        topology.master1.add_s(Entry(("cn=%s,%s" % (name, STAGING_DN), {
                                            'objectclass': "top person".split(),
                                            'sn': name,
                                            'cn': name})))


def test_ticket47787_2(topology):
    '''
    Disable replication so that updates are not replicated
    Delete an entry on M1. Modrdn it on M2 (chg rdn + delold=0 + same superior).
    update a test entry on M2
    Reenable the RA.
    checks that entry was deleted on M2 (with the modified RDN)
    checks that test entry was replicated on M1 (replication M2->M1 not broken by modrdn)
    '''

    _header(topology, "test_ticket47787_2")
    _bind_manager(topology.master1)
    _bind_manager(topology.master2)

    #entry to test the replication is still working
    name = "%s%d" % (NEW_ACCOUNT, MAX_ACCOUNTS - 1)
    test_rdn = "cn=%s" % (name)
    testentry_dn = "%s,%s" % (test_rdn, STAGING_DN)

    name = "%s%d" % (NEW_ACCOUNT, MAX_ACCOUNTS - 2)
    test2_rdn = "cn=%s" % (name)
    testentry2_dn = "%s,%s" % (test2_rdn, STAGING_DN)

    # value of updates to test the replication both ways
    attr = 'description'
    value = 'test_ticket47787_2'

    # entry for the modrdn
    name = "%s%d" % (NEW_ACCOUNT, 1)
    rdn = "cn=%s" % (name)
    entry_dn = "%s,%s" % (rdn, STAGING_DN)

    # created on M1, wait the entry exists on M2
    _check_entry_exists(topology.master2, entry_dn)
    _check_entry_exists(topology.master2, testentry_dn)

    _pause_RAs(topology)

    # Delete 'entry_dn' on M1.
    # dummy update is only have a first CSN before the DEL
    # else the DEL will be in min_csn RUV and make diagnostic a bit more complex
    _mod_entry(topology.master1, testentry2_dn, attr, 'dummy')
    _delete_entry(topology.master1, entry_dn, name)
    _mod_entry(topology.master1, testentry2_dn, attr, value)

    time.sleep(1)  # important to have MOD.csn != DEL.csn

    # MOD 'entry_dn' on M1.
    # dummy update is only have a first CSN before the MOD entry_dn
    # else the DEL will be in min_csn RUV and make diagnostic a bit more complex
    _mod_entry(topology.master2, testentry_dn, attr, 'dummy')
    _mod_entry(topology.master2, entry_dn, attr, value)
    _mod_entry(topology.master2, testentry_dn, attr, value)

    _resume_RAs(topology)

    topology.master1.log.info("\n\n######################### Check DEL replicated on M2 ######################\n")
    loop = 0
    while loop <= 10:
        ent = _find_tombstone(topology.master2, SUFFIX, 'sn', name)
        if ent:
            break
        time.sleep(1)
        loop += 1
    assert loop <= 10
    assert ent

    # the following checks are not necessary
    # as this bug is only for failing replicated MOD (entry_dn) on M1
    #_check_mod_received(topology.master1, SUFFIX, "(%s)" % (test_rdn), attr, value)
    #_check_mod_received(topology.master2, SUFFIX, "(%s)" % (test2_rdn), attr, value)
    #
    #_check_replication(topology, testentry_dn)

    _status_entry_both_server(topology, name=name, desc="DEL M1 - MOD M2", debug=DEBUG_FLAG)

    topology.master1.log.info("\n\n######################### Check MOD replicated on M1 ######################\n")
    loop = 0
    while loop <= 10:
        ent = _find_tombstone(topology.master1, SUFFIX, 'sn', name)
        if ent:
            break
        time.sleep(1)
        loop += 1
    assert loop <= 10
    assert ent
    assert ent.hasAttr(attr)
    assert ent.getValue(attr) == value


def test_ticket47787_final(topology):
    topology.master1.delete()
    topology.master2.delete()
    log.info('Testcase PASSED')


def run_isolated():
    '''
        run_isolated is used to run these test cases independently of a test scheduler (xunit, py.test..)
        To run isolated without py.test, you need to
            - edit this file and comment '@pytest.fixture' line before 'topology' function.
            - set the installation prefix
            - run this program
    '''
    global installation1_prefix
    global installation2_prefix
    installation1_prefix = None
    installation2_prefix = None

    topo = topology(True)
    topo.master1.log.info("\n\n######################### Ticket 47787 ######################\n")
    test_ticket47787_init(topo)

    test_ticket47787_2(topo)

    test_ticket47787_final(topo)


if __name__ == '__main__':
    run_isolated()

