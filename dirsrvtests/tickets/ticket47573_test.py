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
import re
from lib389 import DirSrv, Entry, tools
from lib389.tools import DirSrvTools
from lib389._constants import *
from lib389.properties import *

logging.getLogger(__name__).setLevel(logging.DEBUG)
log = logging.getLogger(__name__)

installation_prefix = None

TEST_REPL_DN = "cn=test_repl, %s" % SUFFIX
ENTRY_DN = "cn=test_entry, %s" % SUFFIX

MUST_OLD = "(postalAddress $ preferredLocale $ telexNumber)"
MAY_OLD  = "(postalCode $ street)"

MUST_NEW = "(postalAddress $ preferredLocale)"
MAY_NEW  = "(telexNumber $ postalCode $ street)"


class TopologyMasterConsumer(object):
    def __init__(self, master, consumer):
        master.open()
        self.master = master

        consumer.open()
        self.consumer = consumer


def pattern_errorlog(file, log_pattern):
    try:
        pattern_errorlog.last_pos += 1
    except AttributeError:
        pattern_errorlog.last_pos = 0

    found = None
    log.debug("_pattern_errorlog: start at offset %d" % pattern_errorlog.last_pos)
    file.seek(pattern_errorlog.last_pos)

    # Use a while true iteration because 'for line in file: hit a
    # python bug that break file.tell()
    while True:
        line = file.readline()
        log.debug("_pattern_errorlog: [%d] %s" % (file.tell(), line))
        found = log_pattern.search(line)
        if ((line == '') or (found)):
            break

    log.debug("_pattern_errorlog: end at offset %d" % file.tell())
    pattern_errorlog.last_pos = file.tell()
    return found


def _oc_definition(oid_ext, name, must=None, may=None):
    oid  = "1.2.3.4.5.6.7.8.9.10.%d" % oid_ext
    desc = 'To test ticket 47573'
    sup  = 'person'
    if not must:
        must = MUST_OLD
    if not may:
        may = MAY_OLD

    new_oc = "( %s  NAME '%s' DESC '%s' SUP %s AUXILIARY MUST %s MAY %s )" % (oid, name, desc, sup, must, may)
    return new_oc


def add_OC(instance, oid_ext, name):
    new_oc = _oc_definition(oid_ext, name)
    instance.schema.add_schema('objectClasses', new_oc)


def mod_OC(instance, oid_ext, name, old_must=None, old_may=None, new_must=None, new_may=None):
    old_oc = _oc_definition(oid_ext, name, old_must, old_may)
    new_oc = _oc_definition(oid_ext, name, new_must, new_may)
    instance.schema.del_schema('objectClasses', old_oc)
    instance.schema.add_schema('objectClasses', new_oc)


def trigger_schema_push(topology):
    """
        It triggers an update on the supplier. This will start a replication
        session and a schema push
    """
    try:
        trigger_schema_push.value += 1
    except AttributeError:
        trigger_schema_push.value = 1
    replace = [(ldap.MOD_REPLACE, 'telephonenumber', str(trigger_schema_push.value))]
    topology.master.modify_s(ENTRY_DN, replace)

    # wait 10 seconds that the update is replicated
    loop = 0
    while loop <= 10:
        try:
            ent = topology.consumer.getEntry(ENTRY_DN, ldap.SCOPE_BASE, "(objectclass=*)", ['telephonenumber'])
            val = ent.telephonenumber or "0"
            if int(val) == trigger_schema_push.value:
                return
            # the expected value is not yet replicated. try again
            time.sleep(1)
            loop += 1
            log.debug("trigger_schema_push: receive %s (expected %d)" % (val, trigger_schema_push.value))
        except ldap.NO_SUCH_OBJECT:
            time.sleep(1)
            loop += 1


@pytest.fixture(scope="module")
def topology(request):
    '''
        This fixture is used to create a replicated topology for the 'module'.
        The replicated topology is MASTER -> Consumer.
    '''
    global installation_prefix

    if installation_prefix:
        args_instance[SER_DEPLOYED_DIR] = installation_prefix

    master   = DirSrv(verbose=False)
    consumer = DirSrv(verbose=False)

    # Args for the master instance
    args_instance[SER_HOST] = HOST_MASTER_1
    args_instance[SER_PORT] = PORT_MASTER_1
    args_instance[SER_SERVERID_PROP] = SERVERID_MASTER_1
    args_master = args_instance.copy()
    master.allocate(args_master)

    # Args for the consumer instance
    args_instance[SER_HOST] = HOST_CONSUMER_1
    args_instance[SER_PORT] = PORT_CONSUMER_1
    args_instance[SER_SERVERID_PROP] = SERVERID_CONSUMER_1
    args_consumer = args_instance.copy()
    consumer.allocate(args_consumer)

    # Get the status of the instance
    instance_master = master.exists()
    instance_consumer = consumer.exists()

    # Remove all the instances
    if instance_master:
        master.delete()
    if instance_consumer:
        consumer.delete()

    # Create the instances
    master.create()
    master.open()
    consumer.create()
    consumer.open()

    #
    # Now prepare the Master-Consumer topology
    #
    # First Enable replication
    master.replica.enableReplication(suffix=SUFFIX, role=REPLICAROLE_MASTER, replicaId=REPLICAID_MASTER_1)
    consumer.replica.enableReplication(suffix=SUFFIX, role=REPLICAROLE_CONSUMER)

    # Initialize the supplier->consumer

    properties = {RA_NAME:      r'meTo_$host:$port',
                  RA_BINDDN:    defaultProperties[REPLICATION_BIND_DN],
                  RA_BINDPW:    defaultProperties[REPLICATION_BIND_PW],
                  RA_METHOD:    defaultProperties[REPLICATION_BIND_METHOD],
                  RA_TRANSPORT_PROT: defaultProperties[REPLICATION_TRANSPORT]}
    repl_agreement = master.agreement.create(suffix=SUFFIX, host=consumer.host, port=consumer.port, properties=properties)

    if not repl_agreement:
        log.fatal("Fail to create a replica agreement")
        sys.exit(1)

    log.debug("%s created" % repl_agreement)
    master.agreement.init(SUFFIX, HOST_CONSUMER_1, PORT_CONSUMER_1)
    master.waitForReplInit(repl_agreement)

    # Check replication is working fine
    if master.testReplication(DEFAULT_SUFFIX, consumer):
        log.info('Replication is working.')
    else:
        log.fatal('Replication is not working.')
        assert False

    # clear the tmp directory
    master.clearTmpDir(__file__)

    # Here we have two instances master and consumer
    # with replication working.
    return TopologyMasterConsumer(master, consumer)


def test_ticket47573_init(topology):
    """
        Initialize the test environment
    """
    log.debug("test_ticket47573_init topology %r (master %r, consumer %r" %
              (topology, topology.master, topology.consumer))
    # the test case will check if a warning message is logged in the
    # error log of the supplier
    topology.master.errorlog_file = open(topology.master.errlog, "r")

    # This entry will be used to trigger attempt of schema push
    topology.master.add_s(Entry((ENTRY_DN, {
                                            'objectclass': "top person".split(),
                                            'sn': 'test_entry',
                                            'cn': 'test_entry'})))


def test_ticket47573_one(topology):
    """
        Summary: Add a custom OC with MUST and MAY
            MUST = postalAddress $ preferredLocale
            MAY  = telexNumber   $ postalCode      $ street

        Final state
            - supplier +OCwithMayAttr
            - consumer +OCwithMayAttr

    """
    log.debug("test_ticket47573_one topology %r (master %r, consumer %r" % (topology, topology.master, topology.consumer))
    # update the schema of the supplier so that it is a superset of
    # consumer. Schema should be pushed
    new_oc = _oc_definition(2, 'OCwithMayAttr',
                            must = MUST_OLD,
                            may  = MAY_OLD)
    topology.master.schema.add_schema('objectClasses', new_oc)

    trigger_schema_push(topology)
    master_schema_csn = topology.master.schema.get_schema_csn()
    consumer_schema_csn = topology.consumer.schema.get_schema_csn()

    # Check the schemaCSN was updated on the consumer
    log.debug("test_ticket47573_one master_schema_csn=%s", master_schema_csn)
    log.debug("ctest_ticket47573_one onsumer_schema_csn=%s", consumer_schema_csn)
    assert master_schema_csn == consumer_schema_csn

    # Check the error log of the supplier does not contain an error
    regex = re.compile("must not be overwritten \(set replication log for additional info\)")
    res = pattern_errorlog(topology.master.errorlog_file, regex)
    assert res is None


def test_ticket47573_two(topology):
    """
        Summary: Change OCwithMayAttr to move a MAY attribute to a MUST attribute


        Final state
            - supplier OCwithMayAttr updated
            - consumer OCwithMayAttr updated

    """

    # Update the objectclass so that a MAY attribute is moved to MUST attribute
    mod_OC(topology.master, 2, 'OCwithMayAttr', old_must=MUST_OLD, new_must=MUST_NEW, old_may=MAY_OLD, new_may=MAY_NEW)

    # now push the scheam
    trigger_schema_push(topology)
    master_schema_csn = topology.master.schema.get_schema_csn()
    consumer_schema_csn = topology.consumer.schema.get_schema_csn()

    # Check the schemaCSN was NOT updated on the consumer
    log.debug("test_ticket47573_two master_schema_csn=%s", master_schema_csn)
    log.debug("test_ticket47573_two consumer_schema_csn=%s", consumer_schema_csn)
    assert master_schema_csn == consumer_schema_csn

    # Check the error log of the supplier does not contain an error
    regex = re.compile("must not be overwritten \(set replication log for additional info\)")
    res = pattern_errorlog(topology.master.errorlog_file, regex)
    assert res is None


def test_ticket47573_three(topology):
    '''
        Create a entry with OCwithMayAttr OC
    '''
    # Check replication is working fine
    dn = "cn=ticket47573, %s" % SUFFIX
    topology.master.add_s(Entry((dn,
                                 {'objectclass': "top person OCwithMayAttr".split(),
                                  'sn':               'test_repl',
                                  'cn':               'test_repl',
                                  'postalAddress':    'here',
                                  'preferredLocale':  'en',
                                  'telexNumber':      '12$us$21',
                                  'postalCode':       '54321'})))
    loop = 0
    ent = None
    while loop <= 10:
        try:
            ent = topology.consumer.getEntry(dn, ldap.SCOPE_BASE, "(objectclass=*)")
            break
        except ldap.NO_SUCH_OBJECT:
            time.sleep(1)
            loop += 1
    if ent is None:
        assert False


def test_ticket47573_final(topology):
    topology.master.delete()
    topology.consumer.delete()
    log.info('Testcase PASSED')


def run_isolated():
    '''
        run_isolated is used to run these test cases independently of a test scheduler (xunit, py.test..)
        To run isolated without py.test, you need to
            - edit this file and comment '@pytest.fixture' line before 'topology' function.
            - set the installation prefix
            - run this program
    '''
    global installation_prefix
    installation_prefix = None

    topo = topology(True)
    test_ticket47573_init(topo)
    test_ticket47573_one(topo)
    test_ticket47573_two(topo)
    test_ticket47573_three(topo)

    test_ticket47573_final(topo)


if __name__ == '__main__':
    run_isolated()

