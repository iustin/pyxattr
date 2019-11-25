#
#

import sys
import unittest
import tempfile
import os
import errno

import xattr
from xattr import NS_USER, XATTR_CREATE, XATTR_REPLACE

NAMESPACE = os.environ.get("NAMESPACE", NS_USER)

if sys.hexversion >= 0x03000000:
    PY3K = True
    EMPTY_NS = bytes()
else:
    PY3K = False
    EMPTY_NS = ''

TEST_DIR = os.environ.get("TEST_DIR", ".")
TEST_IGNORE_XATTRS = os.environ.get("TEST_IGNORE_XATTRS", "")
if TEST_IGNORE_XATTRS == "":
    TEST_IGNORE_XATTRS = []
else:
    TEST_IGNORE_XATTRS = TEST_IGNORE_XATTRS.split(",")
    # The following has to be a list comprehension, not a generator, to
    # avoid weird consequences of lazy evaluation.
    TEST_IGNORE_XATTRS.extend([a.encode() for a in TEST_IGNORE_XATTRS])

USER_NN = "test"
USER_ATTR = NAMESPACE.decode() + "." + USER_NN
USER_VAL = "abc"
EMPTY_VAL = ""
LARGE_VAL = "x" * 2048
MANYOPS_COUNT = 131072

if PY3K:
    USER_NN = USER_NN.encode()
    USER_VAL = USER_VAL.encode()
    USER_ATTR = USER_ATTR.encode()
    EMPTY_VAL = EMPTY_VAL.encode()
    LARGE_VAL = LARGE_VAL.encode()

def ignore_tuples(attrs):
    """Remove ignored attributes from the output of xattr.get_all."""
    return [attr for attr in attrs
            if attr[0] not in TEST_IGNORE_XATTRS]

def ignore(attrs):
    """Remove ignored attributes from the output of xattr.list"""
    return [attr for attr in attrs
            if attr not in TEST_IGNORE_XATTRS]

def lists_equal(attrs, value):
    """Helper to check list equivalence, skipping TEST_IGNORE_XATTRS."""
    assert ignore(attrs) == value

def tuples_equal(attrs, value):
    """Helper to check list equivalence, skipping TEST_IGNORE_XATTRS."""
    assert ignore_tuples(attrs) == value


class xattrTest(unittest.TestCase):
    def setUp(self):
        """set up function"""
        self.rmfiles = []
        self.rmdirs = []

    def tearDown(self):
        """tear down function"""
        for fname in self.rmfiles:
            try:
                os.unlink(fname)
            except EnvironmentError:
                continue
        for dname in self.rmdirs:
            try:
                os.rmdir(dname)
            except EnvironmentError:
                continue

    def _getfile(self):
        """create a temp file"""
        fh, fname = tempfile.mkstemp(".test", "xattr-", TEST_DIR)
        self.rmfiles.append(fname)
        return fh, fname

    def _getdir(self):
        """create a temp dir"""
        dname = tempfile.mkdtemp(".test", "xattr-", TEST_DIR)
        self.rmdirs.append(dname)
        return dname

    def _getsymlink(self, dangling=True):
        """create a symlink"""
        fh, fname = self._getfile()
        os.close(fh)
        if dangling:
            os.unlink(fname)
        sname = fname + ".symlink"
        os.symlink(fname, sname)
        self.rmfiles.append(sname)
        return fname, sname

    def _checkDeprecated(self, item, symlink=False):
        """check deprecated list, set, get operations against an item"""
        lists_equal(xattr.listxattr(item, symlink), [])
        self.assertRaises(EnvironmentError, xattr.setxattr, item,
                          USER_ATTR, USER_VAL,
                          XATTR_REPLACE, symlink)
        xattr.setxattr(item, USER_ATTR, USER_VAL, 0, symlink)
        self.assertRaises(EnvironmentError, xattr.setxattr, item,
                          USER_ATTR, USER_VAL, XATTR_CREATE, symlink)
        lists_equal(xattr.listxattr(item, symlink), [USER_ATTR])
        self.assertEqual(xattr.getxattr(item, USER_ATTR, symlink),
                         USER_VAL)
        tuples_equal(xattr.get_all(item, nofollow=symlink),
                          [(USER_ATTR, USER_VAL)])
        xattr.removexattr(item, USER_ATTR, symlink)
        lists_equal(xattr.listxattr(item, symlink), [])
        tuples_equal(xattr.get_all(item, nofollow=symlink),
                         [])
        self.assertRaises(EnvironmentError, xattr.removexattr,
                          item, USER_ATTR, symlink)

    def _checkListSetGet(self, item, symlink=False, use_ns=False):
        """check list, set, get operations against an item"""
        lists_equal(xattr.list(item, symlink), [])
        self.assertRaises(EnvironmentError, xattr.set, item,
                          USER_ATTR, USER_VAL,
                          flags=XATTR_REPLACE,
                          nofollow=symlink)
        self.assertRaises(EnvironmentError, xattr.set, item,
                          USER_NN, USER_VAL,
                          flags=XATTR_REPLACE,
                          namespace=NAMESPACE,
                          nofollow=symlink)
        if use_ns:
            xattr.set(item, USER_NN, USER_VAL,
                      namespace=NAMESPACE,
                      nofollow=symlink)
        else:
            xattr.set(item, USER_ATTR, USER_VAL,
                      nofollow=symlink)
        self.assertRaises(EnvironmentError, xattr.set, item,
                          USER_ATTR, USER_VAL,
                          flags=XATTR_CREATE,
                          nofollow=symlink)
        self.assertRaises(EnvironmentError, xattr.set, item,
                          USER_NN, USER_VAL,
                          flags=XATTR_CREATE,
                          namespace=NAMESPACE,
                          nofollow=symlink)
        lists_equal(xattr.list(item, nofollow=symlink), [USER_ATTR])
        lists_equal(xattr.list(item, nofollow=symlink,
                                  namespace=EMPTY_NS),
                       [USER_ATTR])
        self.assertEqual(xattr.list(item, namespace=NAMESPACE, nofollow=symlink),
                         [USER_NN])
        self.assertEqual(xattr.get(item, USER_ATTR, nofollow=symlink),
                         USER_VAL)
        self.assertEqual(xattr.get(item, USER_NN, nofollow=symlink,
                                   namespace=NAMESPACE), USER_VAL)
        tuples_equal(xattr.get_all(item, nofollow=symlink),
                         [(USER_ATTR, USER_VAL)])
        self.assertEqual(xattr.get_all(item, nofollow=symlink,
                                       namespace=NAMESPACE),
                         [(USER_NN, USER_VAL)])
        if use_ns:
            xattr.remove(item, USER_NN, namespace=NAMESPACE, nofollow=symlink)
        else:
            xattr.remove(item, USER_ATTR, nofollow=symlink)
        lists_equal(xattr.list(item, nofollow=symlink), [])
        tuples_equal(xattr.get_all(item, nofollow=symlink),
                         [])
        self.assertRaises(EnvironmentError, xattr.remove,
                          item, USER_ATTR, nofollow=symlink)
        self.assertRaises(EnvironmentError, xattr.remove, item,
                          USER_NN, namespace=NAMESPACE, nofollow=symlink)

    def testNoXattrDeprecated(self):
        """test no attributes (deprecated functions)"""
        fh, fname = self._getfile()
        lists_equal(xattr.listxattr(fname), [])
        tuples_equal(xattr.get_all(fname), [])
        self.assertRaises(EnvironmentError, xattr.getxattr, fname,
                              USER_ATTR)
        dname = self._getdir()
        lists_equal(xattr.listxattr(dname), [])
        tuples_equal(xattr.get_all(dname), [])
        self.assertRaises(EnvironmentError, xattr.getxattr, dname,
                              USER_ATTR)
        _, sname = self._getsymlink()
        lists_equal(xattr.listxattr(sname, True), [])
        tuples_equal(xattr.get_all(sname, nofollow=True), [])
        self.assertRaises(EnvironmentError, xattr.getxattr, fname,
                              USER_ATTR, True)


    def testNoXattr(self):
        """test no attributes"""
        fh, fname = self._getfile()
        lists_equal(xattr.list(fname), [])
        self.assertEqual(xattr.list(fname, namespace=NAMESPACE), [])
        tuples_equal(xattr.get_all(fname), [])
        self.assertEqual(xattr.get_all(fname, namespace=NAMESPACE), [])
        self.assertRaises(EnvironmentError, xattr.get, fname,
                              USER_NN, namespace=NAMESPACE)
        dname = self._getdir()
        lists_equal(xattr.list(dname), [])
        self.assertEqual(xattr.list(dname, namespace=NAMESPACE), [])
        tuples_equal(xattr.get_all(dname), [])
        self.assertEqual(xattr.get_all(dname, namespace=NAMESPACE), [])
        self.assertRaises(EnvironmentError, xattr.get, dname,
                              USER_NN, namespace=NAMESPACE)
        _, sname = self._getsymlink()
        lists_equal(xattr.list(sname, nofollow=True), [])
        self.assertEqual(xattr.list(sname, nofollow=True,
                                    namespace=NAMESPACE), [])
        tuples_equal(xattr.get_all(sname, nofollow=True), [])
        self.assertEqual(xattr.get_all(sname, nofollow=True,
                                           namespace=NAMESPACE), [])
        self.assertRaises(EnvironmentError, xattr.get, sname,
                              USER_NN, namespace=NAMESPACE, nofollow=True)

    def testFileByNameDeprecated(self):
        """test set and retrieve one attribute by file name (deprecated)"""
        fh, fname = self._getfile()
        self._checkDeprecated(fname)
        os.close(fh)

    def testFileByName(self):
        """test set and retrieve one attribute by file name"""
        fh, fname = self._getfile()
        self._checkListSetGet(fname)
        self._checkListSetGet(fname, use_ns=True)
        os.close(fh)

    def testFileByDescriptorDeprecated(self):
        """test file descriptor operations (deprecated functions)"""
        fh, fname = self._getfile()
        self._checkDeprecated(fh)
        os.close(fh)

    def testFileByDescriptor(self):
        """test file descriptor operations"""
        fh, fname = self._getfile()
        self._checkListSetGet(fh)
        self._checkListSetGet(fh, use_ns=True)
        os.close(fh)

    def testFileByObjectDeprecated(self):
        """test file descriptor operations (deprecated functions)"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        self._checkDeprecated(fo)
        fo.close()

    def testFileByObject(self):
        """test file descriptor operations"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        self._checkListSetGet(fo)
        self._checkListSetGet(fo, use_ns=True)
        fo.close()

    def testMixedAccessDeprecated(self):
        """test mixed access to file (deprecated functions)"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        lists_equal(xattr.listxattr(fname), [])
        xattr.setxattr(fname, USER_ATTR, USER_VAL)
        lists_equal(xattr.listxattr(fh), [USER_ATTR])
        self.assertEqual(xattr.getxattr(fo, USER_ATTR), USER_VAL)
        tuples_equal(xattr.get_all(fo), [(USER_ATTR, USER_VAL)])
        tuples_equal(xattr.get_all(fname),
                         [(USER_ATTR, USER_VAL)])
        fo.close()

    def testMixedAccess(self):
        """test mixed access to file"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        lists_equal(xattr.list(fname), [])
        xattr.set(fname, USER_ATTR, USER_VAL)
        lists_equal(xattr.list(fh), [USER_ATTR])
        self.assertEqual(xattr.list(fh, namespace=NAMESPACE), [USER_NN])
        self.assertEqual(xattr.get(fo, USER_ATTR), USER_VAL)
        self.assertEqual(xattr.get(fo, USER_NN, namespace=NAMESPACE),
                         USER_VAL)
        tuples_equal(xattr.get_all(fo),
                         [(USER_ATTR, USER_VAL)])
        self.assertEqual(xattr.get_all(fo, namespace=NAMESPACE),
                         [(USER_NN, USER_VAL)])
        tuples_equal(xattr.get_all(fname),
                         [(USER_ATTR, USER_VAL)])
        self.assertEqual(xattr.get_all(fname, namespace=NAMESPACE),
                         [(USER_NN, USER_VAL)])
        fo.close()

    def testDirOpsDeprecated(self):
        """test attribute setting on directories (deprecated functions)"""
        dname = self._getdir()
        self._checkDeprecated(dname)

    def testDirOps(self):
        """test attribute setting on directories"""
        dname = self._getdir()
        self._checkListSetGet(dname)
        self._checkListSetGet(dname, use_ns=True)

    def testBinaryPayloadDeprecated(self):
        """test binary values (deprecated functions)"""
        fh, fname = self._getfile()
        os.close(fh)
        BINVAL = "abc" + '\0' + "def"
        if PY3K:
            BINVAL = BINVAL.encode()
        xattr.setxattr(fname, USER_ATTR, BINVAL)
        lists_equal(xattr.listxattr(fname), [USER_ATTR])
        self.assertEqual(xattr.getxattr(fname, USER_ATTR), BINVAL)
        tuples_equal(xattr.get_all(fname), [(USER_ATTR, BINVAL)])
        xattr.removexattr(fname, USER_ATTR)

    def testBinaryPayload(self):
        """test binary values"""
        fh, fname = self._getfile()
        os.close(fh)
        BINVAL = "abc" + '\0' + "def"
        if PY3K:
            BINVAL = BINVAL.encode()
        xattr.set(fname, USER_ATTR, BINVAL)
        lists_equal(xattr.list(fname), [USER_ATTR])
        self.assertEqual(xattr.list(fname, namespace=NAMESPACE), [USER_NN])
        self.assertEqual(xattr.get(fname, USER_ATTR), BINVAL)
        self.assertEqual(xattr.get(fname, USER_NN,
                                   namespace=NAMESPACE), BINVAL)
        tuples_equal(xattr.get_all(fname), [(USER_ATTR, BINVAL)])
        self.assertEqual(xattr.get_all(fname, namespace=NAMESPACE),
                         [(USER_NN, BINVAL)])
        xattr.remove(fname, USER_ATTR)

    def testManyOpsDeprecated(self):
        """test many ops (deprecated functions)"""
        fh, fname = self._getfile()
        xattr.setxattr(fh, USER_ATTR, USER_VAL)
        VL = [USER_ATTR]
        for i in range(MANYOPS_COUNT):
            lists_equal(xattr.listxattr(fh), VL)
        for i in range(MANYOPS_COUNT):
            self.assertEqual(xattr.getxattr(fh, USER_ATTR), USER_VAL)
        for i in range(MANYOPS_COUNT):
            tuples_equal(xattr.get_all(fh),
                             [(USER_ATTR, USER_VAL)])

    def testManyOps(self):
        """test many ops"""
        fh, fname = self._getfile()
        xattr.set(fh, USER_ATTR, USER_VAL)
        VL = [USER_ATTR]
        VN = [USER_NN]
        for i in range(MANYOPS_COUNT):
            lists_equal(xattr.list(fh), VL)
            lists_equal(xattr.list(fh, namespace=EMPTY_NS), VL)
            self.assertEqual(xattr.list(fh, namespace=NAMESPACE), VN)
        for i in range(MANYOPS_COUNT):
            self.assertEqual(xattr.get(fh, USER_ATTR), USER_VAL)
            self.assertEqual(xattr.get(fh, USER_NN, namespace=NAMESPACE),
                             USER_VAL)
        for i in range(MANYOPS_COUNT):
            tuples_equal(xattr.get_all(fh),
                             [(USER_ATTR, USER_VAL)])
            self.assertEqual(xattr.get_all(fh, namespace=NAMESPACE),
                             [(USER_NN, USER_VAL)])

    def testNoneNamespace(self):
        fh, fname = self._getfile()
        self.assertRaises(TypeError, xattr.get, fh, USER_ATTR,
                          namespace=None)

    def testEmptyValue(self):
        fh, fname = self._getfile()
        xattr.set(fh, USER_ATTR, EMPTY_VAL)
        self.assertEqual(xattr.get(fh, USER_ATTR), EMPTY_VAL)

    def testWrongCall(self):
       for call in [xattr.get,
                    xattr.list, xattr.listxattr,
                    xattr.remove, xattr.removexattr,
                    xattr.set, xattr.setxattr,
                    xattr.get, xattr.getxattr]:
           self.assertRaises(TypeError, call)

    def testWrongType(self):
        self.assertRaises(TypeError, xattr.get, object(), USER_ATTR)
        for call in [xattr.listxattr, xattr.list]:
            self.assertRaises(TypeError, call, object())
        for call in [xattr.remove, xattr.removexattr,
                     xattr.get, xattr.getxattr]:
            self.assertRaises(TypeError, call, object(), USER_ATTR)
        for call in [xattr.set, xattr.setxattr]:
            self.assertRaises(TypeError, call, object(), USER_ATTR, USER_VAL)


    def testLargeAttribute(self):
        fh, fname = self._getfile()

        xattr.set(fh, USER_ATTR, LARGE_VAL)
        self.assertEqual(xattr.get(fh, USER_ATTR), LARGE_VAL)


if __name__ == "__main__":
    unittest.main()
