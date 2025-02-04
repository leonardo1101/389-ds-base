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
MAX_OTHERS = 10

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

    # Get the status of the instance and restart it if it exists
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

    #
    # Here we have two instances master and consumer
    # with replication working. Either coming from a backup recovery
    # or from a fresh (re)init
    # Time to return the topology
    return TopologyMasterConsumer(master, consumer)


def test_ticket47871_init(topology):
    """
        Initialize the test environment
    """
    topology.master.plugins.enable(name=PLUGIN_RETRO_CHANGELOG)
    mod = [(ldap.MOD_REPLACE, 'nsslapd-changelogmaxage', "10s"),  # 10 second triming
           (ldap.MOD_REPLACE, 'nsslapd-changelog-trim-interval', "5s")]
    topology.master.modify_s("cn=%s,%s" % (PLUGIN_RETRO_CHANGELOG, DN_PLUGIN), mod)
    #topology.master.plugins.enable(name=PLUGIN_MEMBER_OF)
    #topology.master.plugins.enable(name=PLUGIN_REFER_INTEGRITY)
    topology.master.stop(timeout=10)
    topology.master.start(timeout=10)

    topology.master.log.info("test_ticket47871_init topology %r" % (topology))
    # the test case will check if a warning message is logged in the
    # error log of the supplier
    topology.master.errorlog_file = open(topology.master.errlog, "r")


def test_ticket47871_1(topology):
    '''
    ADD entries and check they are all in the retrocl
    '''
    # add dummy entries
    for cpt in range(MAX_OTHERS):
        name = "%s%d" % (OTHER_NAME, cpt)
        topology.master.add_s(Entry(("cn=%s,%s" % (name, SUFFIX), {
                                            'objectclass': "top person".split(),
                                            'sn': name,
                                            'cn': name})))

    topology.master.log.info("test_ticket47871_init: %d entries ADDed %s[0..%d]" % (MAX_OTHERS, OTHER_NAME, MAX_OTHERS - 1))

    # Check the number of entries in the retro changelog
    time.sleep(1)
    ents = topology.master.search_s(RETROCL_SUFFIX, ldap.SCOPE_ONELEVEL, "(objectclass=*)")
    assert len(ents) == MAX_OTHERS
    topology.master.log.info("Added entries are")
    for ent in ents:
        topology.master.log.info("%s" % ent.dn)


def test_ticket47871_2(topology):
    '''
    Wait until there is just a last entries
    '''
    MAX_TRIES = 10
    TRY_NO = 1
    while TRY_NO <= MAX_TRIES:
        time.sleep(6)  # at least 1 trimming occurred
        ents = topology.master.search_s(RETROCL_SUFFIX, ldap.SCOPE_ONELEVEL, "(objectclass=*)")
        assert len(ents) <= MAX_OTHERS
        topology.master.log.info("\nTry no %d it remains %d entries" % (TRY_NO, len(ents)))
        for ent in ents:
            topology.master.log.info("%s" % ent.dn)
        if len(ents) > 1:
            TRY_NO += 1
        else:
            break
    assert TRY_NO <= MAX_TRIES
    assert len(ents) <= 1


def test_ticket47871_final(topology):
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
    test_ticket47871_init(topo)
    test_ticket47871_1(topo)
    test_ticket47871_2(topo)

    test_ticket47871_final(topo)


if __name__ == '__main__':
    run_isolated()
