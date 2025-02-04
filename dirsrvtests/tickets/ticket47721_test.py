# --- BEGIN COPYRIGHT BLOCK ---
# Copyright (C) 2015 Red Hat, Inc.
# All rights reserved.
#
# License: GPL (version 3 or any later version).
# See LICENSE for details. 
# --- END COPYRIGHT BLOCK ---
#
'''
Created on Nov 7, 2013

@author: tbordaz
'''
import os
import sys
import time
import ldap
import logging
import pytest
from lib389 import DirSrv, Entry, tools
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

SCHEMA_DN    = "cn=schema"
TEST_REPL_DN = "cn=test_repl, %s" % SUFFIX
OC_NAME      = 'OCticket47721'
OC_OID_EXT   = 2
MUST = "(postalAddress $ postalCode)"
MAY  = "(member $ street)"

OC2_NAME    = 'OC2ticket47721'
OC2_OID_EXT = 3
MUST_2 = "(postalAddress $ postalCode)"
MAY_2  = "(member $ street)"

REPL_SCHEMA_POLICY_CONSUMER = "cn=consumerUpdatePolicy,cn=replSchema,cn=config"
REPL_SCHEMA_POLICY_SUPPLIER = "cn=supplierUpdatePolicy,cn=replSchema,cn=config"

OTHER_NAME = 'other_entry'
MAX_OTHERS = 10

BIND_NAME  = 'bind_entry'
BIND_DN    = 'cn=%s, %s' % (BIND_NAME, SUFFIX)
BIND_PW    = 'password'

ENTRY_NAME = 'test_entry'
ENTRY_DN   = 'cn=%s, %s' % (ENTRY_NAME, SUFFIX)
ENTRY_OC   = "top person %s" % OC_NAME

BASE_OID = "1.2.3.4.5.6.7.8.9.10"

SLEEP_INTERVAL = 60

def _add_custom_at_definition(name='ATticket47721'):
    new_at = "( %s-oid NAME '%s' DESC 'test AT ticket 47721' SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 X-ORIGIN ( 'Test 47721' 'user defined' ) )" % (name, name)
    return new_at


def _chg_std_at_defintion():
    new_at = "( 2.16.840.1.113730.3.1.569 NAME 'cosPriority' DESC 'Netscape defined attribute type' SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 X-ORIGIN 'Netscape Directory Server' )"
    return new_at


def _add_custom_oc_defintion(name='OCticket47721'):
    new_oc = "( %s-oid NAME '%s' DESC 'An group of related automount objects' SUP top STRUCTURAL MUST ou X-ORIGIN 'draft-howard-rfc2307bis' )" % (name, name)
    return new_oc


def _chg_std_oc_defintion():
    new_oc = "( 5.3.6.1.1.1.2.0 NAME 'trustAccount' DESC 'Sets trust accounts information' SUP top AUXILIARY MUST trustModel MAY ( accessTo $ ou ) X-ORIGIN 'nss_ldap/pam_ldap' )"
    return new_oc


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

    #
    # Here we have two instances master and consumer
    # with replication working. Either coming from a backup recovery
    # or from a fresh (re)init
    # Time to return the topology
    return TopologyMaster1Master2(master1, master2)


def test_ticket47721_init(topology):
    """
        It adds
           - Objectclass with MAY 'member'
           - an entry ('bind_entry') with which we bind to test the 'SELFDN' operation
        It deletes the anonymous aci

    """

    # entry used to bind with
    topology.master1.log.info("Add %s" % BIND_DN)
    topology.master1.add_s(Entry((BIND_DN, {
                                            'objectclass': "top person".split(),
                                            'sn':           BIND_NAME,
                                            'cn':           BIND_NAME,
                                            'userpassword': BIND_PW})))

    # enable repl error logging
    mod = [(ldap.MOD_REPLACE, 'nsslapd-errorlog-level', str(8192))]  # REPL logging
    topology.master1.modify_s(DN_CONFIG, mod)
    topology.master2.modify_s(DN_CONFIG, mod)

    # add dummy entries
    for cpt in range(MAX_OTHERS):
        name = "%s%d" % (OTHER_NAME, cpt)
        topology.master1.add_s(Entry(("cn=%s,%s" % (name, SUFFIX), {
                                            'objectclass': "top person".split(),
                                            'sn': name,
                                            'cn': name})))


def test_ticket47721_0(topology):
    dn = "cn=%s0,%s" % (OTHER_NAME, SUFFIX)
    loop = 0
    ent = None
    while loop <= 10:
        try:
            ent = topology.master2.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            break
        except ldap.NO_SUCH_OBJECT:
            time.sleep(1)
            loop += 1
    if ent is None:
        assert False


def test_ticket47721_1(topology):
    log.info('Running test 1...')
    #topology.master1.log.info("Attach debugger\n\n")
    #time.sleep(30)

    new = _add_custom_at_definition()
    topology.master1.log.info("Add (M2) %s " % new)
    topology.master2.schema.add_schema('attributetypes', new)

    new = _chg_std_at_defintion()
    topology.master1.log.info("Chg (M2) %s " % new)
    topology.master2.schema.add_schema('attributetypes', new)

    new = _add_custom_oc_defintion()
    topology.master1.log.info("Add (M2) %s " % new)
    topology.master2.schema.add_schema('objectClasses', new)

    new = _chg_std_oc_defintion()
    topology.master1.log.info("Chg (M2) %s " % new)
    topology.master2.schema.add_schema('objectClasses', new)

    mod = [(ldap.MOD_REPLACE, 'description', 'Hello world 1')]
    dn = "cn=%s0,%s" % (OTHER_NAME, SUFFIX)
    topology.master2.modify_s(dn, mod)

    loop = 0
    while loop <= 10:
        try:
            ent = topology.master1.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            if ent.hasAttr('description') and (ent.getValue('description') == 'Hello world 1'):
                break
        except ldap.NO_SUCH_OBJECT:
            loop += 1
        time.sleep(1)
    assert loop <= 10

    time.sleep(2)
    schema_csn_master1 = topology.master1.schema.get_schema_csn()
    schema_csn_master2 = topology.master2.schema.get_schema_csn()
    log.debug('Master 1 schemaCSN: %s' % schema_csn_master1)
    log.debug('Master 2 schemaCSN: %s' % schema_csn_master2)


def test_ticket47721_2(topology):
    log.info('Running test 2...')

    mod = [(ldap.MOD_REPLACE, 'description', 'Hello world 2')]
    dn = "cn=%s0,%s" % (OTHER_NAME, SUFFIX)
    topology.master1.modify_s(dn, mod)

    loop = 0
    while loop <= 10:
        try:
            ent = topology.master2.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            if ent.hasAttr('description') and (ent.getValue('description') == 'Hello world 2'):
                break
        except ldap.NO_SUCH_OBJECT:
            loop += 1
        time.sleep(1)
    assert loop <= 10

    time.sleep(2)
    schema_csn_master1 = topology.master1.schema.get_schema_csn()
    schema_csn_master2 = topology.master2.schema.get_schema_csn()
    log.debug('Master 1 schemaCSN: %s' % schema_csn_master1)
    log.debug('Master 2 schemaCSN: %s' % schema_csn_master2)
    if schema_csn_master1 != schema_csn_master2:
        # We need to give the server a little more time, then check it again
        log.info('Schema CSNs are not in sync yet: m1 (%s) vs m2 (%s), wait a little...'
            % (schema_csn_master1, schema_csn_master2))
        time.sleep(SLEEP_INTERVAL)
        schema_csn_master1 = topology.master1.schema.get_schema_csn()
        schema_csn_master2 = topology.master2.schema.get_schema_csn()

    assert schema_csn_master1 is not None
    assert schema_csn_master1 == schema_csn_master2


def test_ticket47721_3(topology):
    '''
    Check that the supplier can update its schema from consumer schema
    Update M2 schema, then trigger a replication M1->M2
    '''
    log.info('Running test 3...')

    # stop RA M2->M1, so that M1 can only learn being a supplier
    ents = topology.master2.agreement.list(suffix=SUFFIX)
    assert len(ents) == 1
    topology.master2.agreement.pause(ents[0].dn)

    new = _add_custom_at_definition('ATtest3')
    topology.master1.log.info("Update schema (M2) %s " % new)
    topology.master2.schema.add_schema('attributetypes', new)

    new = _add_custom_oc_defintion('OCtest3')
    topology.master1.log.info("Update schema (M2) %s " % new)
    topology.master2.schema.add_schema('objectClasses', new)

    mod = [(ldap.MOD_REPLACE, 'description', 'Hello world 3')]
    dn = "cn=%s0,%s" % (OTHER_NAME, SUFFIX)
    topology.master1.modify_s(dn, mod)

    loop = 0
    while loop <= 10:
        try:
            ent = topology.master2.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            if ent.hasAttr('description') and (ent.getValue('description') == 'Hello world 3'):
                break
        except ldap.NO_SUCH_OBJECT:
            loop += 1
        time.sleep(1)
    assert loop <= 10

    time.sleep(2)
    schema_csn_master1 = topology.master1.schema.get_schema_csn()
    schema_csn_master2 = topology.master2.schema.get_schema_csn()
    log.debug('Master 1 schemaCSN: %s' % schema_csn_master1)
    log.debug('Master 2 schemaCSN: %s' % schema_csn_master2)
    if schema_csn_master1 == schema_csn_master2:
        # We need to give the server a little more time, then check it again
        log.info('Schema CSNs are not in sync yet: m1 (%s) vs m2 (%s), wait a little...'
            % (schema_csn_master1, schema_csn_master2))
        time.sleep(SLEEP_INTERVAL)
        schema_csn_master1 = topology.master1.schema.get_schema_csn()
        schema_csn_master2 = topology.master2.schema.get_schema_csn()

    assert schema_csn_master1 is not None
    # schema csn on M2 is larger that on M1. M1 only took the new definitions
    assert schema_csn_master1 != schema_csn_master2


def test_ticket47721_4(topology):
    '''
    Here M2->M1 agreement is disabled.
    with test_ticket47721_3, M1 schema and M2 should be identical BUT
    the nsschemacsn is M2>M1. But as the RA M2->M1 is disabled, M1 keeps its schemacsn.
    Update schema on M2 (nsschemaCSN update), update M2. Check they have the same schemacsn
    '''
    log.info('Running test 4...')

    new = _add_custom_at_definition('ATtest4')
    topology.master1.log.info("Update schema (M1) %s " % new)
    topology.master1.schema.add_schema('attributetypes', new)

    new = _add_custom_oc_defintion('OCtest4')
    topology.master1.log.info("Update schema (M1) %s " % new)
    topology.master1.schema.add_schema('objectClasses', new)

    topology.master1.log.info("trigger replication M1->M2: to update the schema")
    mod = [(ldap.MOD_REPLACE, 'description', 'Hello world 4')]
    dn = "cn=%s0,%s" % (OTHER_NAME, SUFFIX)
    topology.master1.modify_s(dn, mod)

    loop = 0
    while loop <= 10:
        try:
            ent = topology.master2.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            if ent.hasAttr('description') and (ent.getValue('description') == 'Hello world 4'):
                break
        except ldap.NO_SUCH_OBJECT:
            loop += 1
        time.sleep(1)
    assert loop <= 10

    topology.master1.log.info("trigger replication M1->M2: to push the schema")
    mod = [(ldap.MOD_REPLACE, 'description', 'Hello world 5')]
    dn = "cn=%s0,%s" % (OTHER_NAME, SUFFIX)
    topology.master1.modify_s(dn, mod)

    loop = 0
    while loop <= 10:
        try:
            ent = topology.master2.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            if ent.hasAttr('description') and (ent.getValue('description') == 'Hello world 5'):
                break
        except ldap.NO_SUCH_OBJECT:
            loop += 1
        time.sleep(1)
    assert loop <= 10

    time.sleep(2)
    schema_csn_master1 = topology.master1.schema.get_schema_csn()
    schema_csn_master2 = topology.master2.schema.get_schema_csn()
    log.debug('Master 1 schemaCSN: %s' % schema_csn_master1)
    log.debug('Master 2 schemaCSN: %s' % schema_csn_master2)
    if schema_csn_master1 != schema_csn_master2:
        # We need to give the server a little more time, then check it again
        log.info('Schema CSNs are incorrectly in sync, wait a little...')
        time.sleep(SLEEP_INTERVAL)
        schema_csn_master1 = topology.master1.schema.get_schema_csn()
        schema_csn_master2 = topology.master2.schema.get_schema_csn()

    assert schema_csn_master1 is not None
    assert schema_csn_master1 == schema_csn_master2


def test_ticket47721_final(topology):
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
    topo.master1.log.info("\n\n######################### Ticket 47721 ######################\n")
    test_ticket47721_init(topo)

    test_ticket47721_0(topo)
    test_ticket47721_1(topo)
    test_ticket47721_2(topo)
    test_ticket47721_3(topo)
    test_ticket47721_4(topo)

    test_ticket47721_final(topo)


if __name__ == '__main__':
    run_isolated()
