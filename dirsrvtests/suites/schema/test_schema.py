# --- BEGIN COPYRIGHT BLOCK ---
# Copyright (C) 2015 Red Hat, Inc.
# All rights reserved.
#
# License: GPL (version 3 or any later version).
# See LICENSE for details.
# --- END COPYRIGHT BLOCK ---
#
'''
Created on Dec 18, 2013

@author: rmeggins
'''
import os
import sys
import time
import ldap
import six
from ldap.cidict import cidict
from ldap.schema import SubSchema
import logging
import pytest
from lib389 import DirSrv, Entry, tools
from lib389.tools import DirSrvTools
from lib389._constants import *
from lib389.properties import *


logging.getLogger(__name__).setLevel(logging.DEBUG)
log = logging.getLogger(__name__)

installation_prefix = None

attrclass = ldap.schema.models.AttributeType
occlass = ldap.schema.models.ObjectClass
syntax_len_supported = False


class TopologyStandalone(object):
    def __init__(self, standalone):
        standalone.open()
        self.standalone = standalone


@pytest.fixture(scope="module")
def topology(request):
    '''
        This fixture is used to create a DirSrv instance for the 'module'.
    '''
    global installation_prefix

    if installation_prefix:
        args_instance[SER_DEPLOYED_DIR] = installation_prefix
    schemainst = DirSrv(verbose=False)

    # Args for the master instance
    args_instance[SER_HOST] = HOST_STANDALONE
    args_instance[SER_PORT] = PORT_STANDALONE
    args_instance[SER_SERVERID_PROP] = SERVERID_STANDALONE
    schemainst.allocate(args_instance)

    # Remove all the instance
    if schemainst.exists():
        schemainst.delete()

    # Create the instance
    schemainst.create()
    schemainst.open()

    return TopologyStandalone(schemainst)


def ochasattr(subschema, oc, mustormay, attr, key):
    """See if the oc and any of its parents and ancestors have the
    given attr"""
    rc = False
    if not key in oc.__dict__:
        dd = cidict()
        for ii in oc.__dict__[mustormay]:
            dd[ii] = ii
        oc.__dict__[key] = dd
    if attr in oc.__dict__[key]:
        rc = True
    else:
        # look in parents
        for noroid in oc.sup:
            ocpar = subschema.get_obj(occlass, noroid)
            assert(ocpar)
            rc = ochasattr(subschema, ocpar, mustormay, attr, key)
            if rc:
                break
    return rc


def ochasattrs(subschema, oc, mustormay, attrs):
    key = mustormay + "dict"
    ret = []
    for attr in attrs:
        if not ochasattr(subschema, oc, mustormay, attr, key):
            ret.append(attr)
    return ret


def mycmp(v1, v2):
    v1ary, v2ary = [v1], [v2]
    if isinstance(v1, list) or isinstance(v1, tuple):
        v1ary, v2ary = list(set([x.lower() for x in v1])), list(set([x.lower() for x in v2]))
    if not len(v1ary) == len(v2ary):
        return False
    for v1, v2 in zip(v1ary, v2ary):
        if isinstance(v1, six.string_types):
            if not len(v1) == len(v2):
                return False
        if not v1 == v2:
            return False
    return True


def ocgetdiffs(ldschema, oc1, oc2):
    fields = ['obsolete', 'names', 'desc', 'must', 'may', 'kind', 'sup']
    ret = ''
    for field in fields:
        v1, v2 = oc1.__dict__[field], oc2.__dict__[field]
        if field == 'may' or field == 'must':
            missing = ochasattrs(ldschema, oc1, field, oc2.__dict__[field])
            if missing:
                ret = ret + '\t%s is missing %s\n' % (field, missing)
            missing = ochasattrs(ldschema, oc2, field, oc1.__dict__[field])
            if missing:
                ret = ret + '\t%s is missing %s\n' % (field, missing)
        elif not mycmp(v1, v2):
            ret = ret + '\t%s differs: [%s] vs. [%s]\n' % (field, oc1.__dict__[field], oc2.__dict__[field])
    return ret


def atgetparfield(subschema, at, field):
    v = None
    for nameoroid in at.sup:
        atpar = subschema.get_obj(attrclass, nameoroid)
        assert(atpar)
        v = atpar.__dict__.get(field, atgetparfield(subschema, atpar, field))
        if v is not None:
            break
    return v


def atgetdiffs(ldschema, at1, at2):
    fields = ['names', 'desc', 'obsolete', 'sup', 'equality', 'ordering', 'substr', 'syntax',
              'single_value', 'collective', 'no_user_mod', 'usage']
    if syntax_len_supported:
        fields.append('syntax_len')
    ret = ''
    for field in fields:
        v1 = at1.__dict__.get(field) or atgetparfield(ldschema, at1, field)
        v2 = at2.__dict__.get(field) or atgetparfield(ldschema, at2, field)
        if not mycmp(v1, v2):
            ret = ret + '\t%s differs: [%s] vs. [%s]\n' % (field, at1.__dict__[field], at2.__dict__[field])
    return ret


def test_schema_comparewithfiles(topology):
    '''Compare the schema from ldap cn=schema with the schema files'''

    log.info('Running test_schema_comparewithfiles...')

    retval = True
    schemainst = topology.standalone
    ldschema = schemainst.schema.get_subschema()
    assert ldschema
    for fn in schemainst.schema.list_files():
        fschema = schemainst.schema.file_to_subschema(fn)
        if not fschema:
            log.warn("Unable to parse %s as a schema file - skipping" % fn)
            continue
        assert fschema
        for oid in fschema.listall(occlass):
            se = fschema.get_obj(occlass, oid)
            assert se
            ldse = ldschema.get_obj(occlass, oid)
            if not ldse:
                log.error("objectclass in %s but not in %s: %s" % (fn, DN_SCHEMA, se))
                retval = False
                continue
            ret = ocgetdiffs(ldschema, ldse, se)
            if ret:
                log.error("name %s oid %s\n%s" % (se.names[0], oid, ret))
                retval = False
        for oid in fschema.listall(attrclass):
            se = fschema.get_obj(attrclass, oid)
            assert se
            ldse = ldschema.get_obj(attrclass, oid)
            if not ldse:
                log.error("attributetype in %s but not in %s: %s" % (fn, DN_SCHEMA, se))
                retval = False
                continue
            ret = atgetdiffs(ldschema, ldse, se)
            if ret:
                log.error("name %s oid %s\n%s" % (se.names[0], oid, ret))
                retval = False
    assert retval

    log.info('test_schema_comparewithfiles: PASSED')


def test_schema_final(topology):
    topology.standalone.delete()


def run_isolated():
    '''
        run_isolated is used to run these test cases independently of a test scheduler (xunit, py.test..)
        To run isolated without py.test, you need to
            - edit this file and comment '@pytest.fixture' line before 'topology' function.
            - set the installation prefix
            - run this program
    '''
    global installation_prefix
    installation_prefix = os.environ.get('PREFIX')

    topo = topology(True)

    test_schema_comparewithfiles(topo)

    test_schema_final(topo)

if __name__ == '__main__':
    run_isolated()

