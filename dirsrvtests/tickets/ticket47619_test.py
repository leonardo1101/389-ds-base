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

logging.getLogger(__name__).setLevel(logging.DEBUG)
log = logging.getLogger(__name__)

installation_prefix = None

TEST_REPL_DN = "cn=test_repl, %s" % SUFFIX
ENTRY_DN = "cn=test_entry, %s" % SUFFIX

OTHER_NAME = 'other_entry'
MAX_OTHERS = 100

ATTRIBUTES = ['street', 'countryName', 'description', 'postalAddress', 'postalCode', 'title', 'l', 'roomNumber']


class TopologyMasterConsumer(object):
    def __init__(self, master, consumer):
        master.open()
        self.master = master

        consumer.open()
        self.consumer = consumer

    def __repr__(self):
            return "Master[%s] -> Consumer[%s" % (self.master, self.consumer)


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
    instance_master   = master.exists()
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


def test_ticket47619_init(topology):
    """
        Initialize the test environment
    """
    topology.master.plugins.enable(name=PLUGIN_RETRO_CHANGELOG)
    #topology.master.plugins.enable(name=PLUGIN_MEMBER_OF)
    #topology.master.plugins.enable(name=PLUGIN_REFER_INTEGRITY)
    topology.master.stop(timeout=10)
    topology.master.start(timeout=10)

    topology.master.log.info("test_ticket47619_init topology %r" % (topology))
    # the test case will check if a warning message is logged in the
    # error log of the supplier
    topology.master.errorlog_file = open(topology.master.errlog, "r")

    # add dummy entries
    for cpt in range(MAX_OTHERS):
        name = "%s%d" % (OTHER_NAME, cpt)
        topology.master.add_s(Entry(("cn=%s,%s" % (name, SUFFIX), {
                                            'objectclass': "top person".split(),
                                            'sn': name,
                                            'cn': name})))

    topology.master.log.info("test_ticket47619_init: %d entries ADDed %s[0..%d]" % (MAX_OTHERS, OTHER_NAME, MAX_OTHERS-1))

    # Check the number of entries in the retro changelog
    time.sleep(2)
    ents = topology.master.search_s(RETROCL_SUFFIX, ldap.SCOPE_ONELEVEL, "(objectclass=*)")
    assert len(ents) == MAX_OTHERS


def test_ticket47619_create_index(topology):
    args = {INDEX_TYPE: 'eq'}
    for attr in ATTRIBUTES:
        topology.master.index.create(suffix=RETROCL_SUFFIX, attr=attr, args=args)


def test_ticket47619_reindex(topology):
    '''
    Reindex all the attributes in ATTRIBUTES
    '''
    args = {TASK_WAIT: True}
    for attr in ATTRIBUTES:
        rc = topology.master.tasks.reindex(suffix=RETROCL_SUFFIX, attrname=attr, args=args)
        assert rc == 0


def test_ticket47619_check_indexed_search(topology):
    for attr in ATTRIBUTES:
        ents = topology.master.search_s(RETROCL_SUFFIX, ldap.SCOPE_SUBTREE, "(%s=hello)" % attr)
        assert len(ents) == 0


def test_ticket47619_final(topology):
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
    test_ticket47619_init(topo)

    test_ticket47619_create_index(topo)

    # important restart that trigger the hang
    # at restart, finding the new 'changelog' backend, the backend is acquired in Read
    # preventing the reindex task to complete
    topo.master.restart(timeout=10)
    test_ticket47619_reindex(topo)
    test_ticket47619_check_indexed_search(topo)

    test_ticket47619_final(topo)


if __name__ == '__main__':
    run_isolated()

