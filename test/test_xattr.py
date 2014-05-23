#
#

import sys
import unittest
import tempfile
import os
import errno

import xattr
from xattr import NS_USER, XATTR_CREATE, XATTR_REPLACE

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

class xattrTest(unittest.TestCase):
    USER_NN = "test"
    USER_ATTR = NS_USER.decode() + "." + USER_NN
    USER_VAL = "abc"
    MANYOPS_COUNT = 131072

    if PY3K:
        USER_NN = USER_NN.encode()
        USER_VAL = USER_VAL.encode()
        USER_ATTR = USER_ATTR.encode()

    @staticmethod
    def _ignore_tuples(attrs):
        """Remove ignored attributes from the output of xattr.get_all."""
        return [attr for attr in attrs
                if attr[0] not in TEST_IGNORE_XATTRS]

    @staticmethod
    def _ignore(attrs):
        """Remove ignored attributes from the output of xattr.list"""
        return [attr for attr in attrs
                if attr not in TEST_IGNORE_XATTRS]

    def checkList(self, attrs, value):
        """Helper to check attribute list equivalence, skipping TEST_IGNORE_XATTRS."""
        self.assertEqual(self._ignore(attrs), value)

    def checkTuples(self, attrs, value):
        """Helper to check attribute list equivalence, skipping TEST_IGNORE_XATTRS."""
        self.assertEqual(self._ignore_tuples(attrs), value)

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
        self.checkList(xattr.listxattr(item, symlink), [])
        self.assertRaises(EnvironmentError, xattr.setxattr, item,
                          self.USER_ATTR, self.USER_VAL,
                          XATTR_REPLACE)
        try:
            xattr.setxattr(item, self.USER_ATTR, self.USER_VAL, 0, symlink)
        except IOError:
            err = sys.exc_info()[1]
            if err.errno == errno.EPERM and symlink:
                # symlinks may fail, in which case we abort the rest
                # of the test for this case
                return
            raise
        self.assertRaises(EnvironmentError, xattr.setxattr, item,
                          self.USER_ATTR, self.USER_VAL, XATTR_CREATE)
        self.checkList(xattr.listxattr(item, symlink), [self.USER_ATTR])
        self.assertEqual(xattr.getxattr(item, self.USER_ATTR, symlink),
                         self.USER_VAL)
        self.checkTuples(xattr.get_all(item, nofollow=symlink),
                          [(self.USER_ATTR, self.USER_VAL)])
        xattr.removexattr(item, self.USER_ATTR)
        self.checkList(xattr.listxattr(item, symlink), [])
        self.checkTuples(xattr.get_all(item, nofollow=symlink),
                         [])
        self.assertRaises(EnvironmentError, xattr.removexattr,
                          item, self.USER_ATTR)

    def _checkListSetGet(self, item, symlink=False, use_ns=False):
        """check list, set, get operations against an item"""
        self.checkList(xattr.list(item, symlink), [])
        self.assertRaises(EnvironmentError, xattr.set, item,
                          self.USER_ATTR, self.USER_VAL, flags=XATTR_REPLACE)
        self.assertRaises(EnvironmentError, xattr.set, item,
                          self.USER_NN, self.USER_VAL, flags=XATTR_REPLACE,
                          namespace=NS_USER)
        try:
            if use_ns:
                xattr.set(item, self.USER_NN, self.USER_VAL,
                          namespace=NS_USER,
                          nofollow=symlink)
            else:
                xattr.set(item, self.USER_ATTR, self.USER_VAL,
                          nofollow=symlink)
        except IOError:
            err = sys.exc_info()[1]
            if err.errno == errno.EPERM and symlink:
                # symlinks may fail, in which case we abort the rest
                # of the test for this case
                return
            raise
        self.assertRaises(EnvironmentError, xattr.set, item,
                          self.USER_ATTR, self.USER_VAL, flags=XATTR_CREATE)
        self.assertRaises(EnvironmentError, xattr.set, item,
                          self.USER_NN, self.USER_VAL,
                          flags=XATTR_CREATE, namespace=NS_USER)
        self.checkList(xattr.list(item, nofollow=symlink), [self.USER_ATTR])
        self.checkList(xattr.list(item, nofollow=symlink,
                                   namespace=EMPTY_NS),
                         [self.USER_ATTR])
        self.assertEqual(xattr.list(item, namespace=NS_USER, nofollow=symlink),
                         [self.USER_NN])
        self.assertEqual(xattr.get(item, self.USER_ATTR, nofollow=symlink),
                         self.USER_VAL)
        self.assertEqual(xattr.get(item, self.USER_NN, nofollow=symlink,
                                   namespace=NS_USER), self.USER_VAL)
        self.checkTuples(xattr.get_all(item, nofollow=symlink),
                         [(self.USER_ATTR, self.USER_VAL)])
        self.assertEqual(xattr.get_all(item, nofollow=symlink,
                                       namespace=NS_USER),
                         [(self.USER_NN, self.USER_VAL)])
        if use_ns:
            xattr.remove(item, self.USER_NN, namespace=NS_USER)
        else:
            xattr.remove(item, self.USER_ATTR)
        self.checkList(xattr.list(item, symlink), [])
        self.checkTuples(xattr.get_all(item, nofollow=symlink),
                         [])
        self.assertRaises(EnvironmentError, xattr.remove,
                          item, self.USER_ATTR, nofollow=symlink)
        self.assertRaises(EnvironmentError, xattr.remove, item,
                          self.USER_NN, namespace=NS_USER, nofollow=symlink)

    def testNoXattrDeprecated(self):
        """test no attributes (deprecated functions)"""
        fh, fname = self._getfile()
        self.checkList(xattr.listxattr(fname), [])
        self.checkTuples(xattr.get_all(fname), [])
        dname = self._getdir()
        self.checkList(xattr.listxattr(dname), [])
        self.checkTuples(xattr.get_all(dname), [])
        _, sname = self._getsymlink()
        self.checkList(xattr.listxattr(sname, True), [])
        self.checkTuples(xattr.get_all(sname, nofollow=True), [])

    def testNoXattr(self):
        """test no attributes"""
        fh, fname = self._getfile()
        self.checkList(xattr.list(fname), [])
        self.assertEqual(xattr.list(fname, namespace=NS_USER), [])
        self.checkTuples(xattr.get_all(fname), [])
        self.assertEqual(xattr.get_all(fname, namespace=NS_USER), [])
        dname = self._getdir()
        self.checkList(xattr.list(dname), [])
        self.assertEqual(xattr.list(dname, namespace=NS_USER), [])
        self.checkTuples(xattr.get_all(dname), [])
        self.assertEqual(xattr.get_all(dname, namespace=NS_USER), [])
        _, sname = self._getsymlink()
        self.checkList(xattr.list(sname, nofollow=True), [])
        self.assertEqual(xattr.list(sname, nofollow=True,
                                    namespace=NS_USER), [])
        self.checkTuples(xattr.get_all(sname, nofollow=True), [])
        self.assertEqual(xattr.get_all(sname, nofollow=True,
                                           namespace=NS_USER), [])

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
        self.checkList(xattr.listxattr(fname), [])
        xattr.setxattr(fname, self.USER_ATTR, self.USER_VAL)
        self.checkList(xattr.listxattr(fh), [self.USER_ATTR])
        self.assertEqual(xattr.getxattr(fo, self.USER_ATTR), self.USER_VAL)
        self.checkTuples(xattr.get_all(fo), [(self.USER_ATTR, self.USER_VAL)])
        self.checkTuples(xattr.get_all(fname),
                         [(self.USER_ATTR, self.USER_VAL)])
        fo.close()

    def testMixedAccess(self):
        """test mixed access to file"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        self.checkList(xattr.list(fname), [])
        xattr.set(fname, self.USER_ATTR, self.USER_VAL)
        self.checkList(xattr.list(fh), [self.USER_ATTR])
        self.assertEqual(xattr.list(fh, namespace=NS_USER), [self.USER_NN])
        self.assertEqual(xattr.get(fo, self.USER_ATTR), self.USER_VAL)
        self.assertEqual(xattr.get(fo, self.USER_NN, namespace=NS_USER),
                         self.USER_VAL)
        self.checkTuples(xattr.get_all(fo),
                         [(self.USER_ATTR, self.USER_VAL)])
        self.assertEqual(xattr.get_all(fo, namespace=NS_USER),
                         [(self.USER_NN, self.USER_VAL)])
        self.checkTuples(xattr.get_all(fname),
                         [(self.USER_ATTR, self.USER_VAL)])
        self.assertEqual(xattr.get_all(fname, namespace=NS_USER),
                         [(self.USER_NN, self.USER_VAL)])
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

    def testSymlinkOpsDeprecated(self):
        """test symlink operations (deprecated functions)"""
        _, sname = self._getsymlink()
        self.assertRaises(EnvironmentError, xattr.listxattr, sname)
        self._checkDeprecated(sname, symlink=True)
        target, sname = self._getsymlink(dangling=False)
        xattr.setxattr(target, self.USER_ATTR, self.USER_VAL)
        self.checkList(xattr.listxattr(target), [self.USER_ATTR])
        self.checkList(xattr.listxattr(sname, True), [])
        self.assertRaises(EnvironmentError, xattr.removexattr, sname,
                          self.USER_ATTR, True)
        xattr.removexattr(sname, self.USER_ATTR, False)

    def testSymlinkOps(self):
        """test symlink operations"""
        _, sname = self._getsymlink()
        self.assertRaises(EnvironmentError, xattr.list, sname)
        self._checkListSetGet(sname, symlink=True)
        self._checkListSetGet(sname, symlink=True, use_ns=True)
        target, sname = self._getsymlink(dangling=False)
        xattr.set(target, self.USER_ATTR, self.USER_VAL)
        self.checkList(xattr.list(target), [self.USER_ATTR])
        self.checkList(xattr.list(sname, nofollow=True), [])
        self.assertRaises(EnvironmentError, xattr.remove, sname,
                          self.USER_ATTR, nofollow=True)
        xattr.remove(sname, self.USER_ATTR, nofollow=False)

    def testBinaryPayloadDeprecated(self):
        """test binary values (deprecated functions)"""
        fh, fname = self._getfile()
        os.close(fh)
        BINVAL = "abc" + '\0' + "def"
        if PY3K:
            BINVAL = BINVAL.encode()
        xattr.setxattr(fname, self.USER_ATTR, BINVAL)
        self.checkList(xattr.listxattr(fname), [self.USER_ATTR])
        self.assertEqual(xattr.getxattr(fname, self.USER_ATTR), BINVAL)
        self.checkTuples(xattr.get_all(fname), [(self.USER_ATTR, BINVAL)])
        xattr.removexattr(fname, self.USER_ATTR)

    def testBinaryPayload(self):
        """test binary values"""
        fh, fname = self._getfile()
        os.close(fh)
        BINVAL = "abc" + '\0' + "def"
        if PY3K:
            BINVAL = BINVAL.encode()
        xattr.set(fname, self.USER_ATTR, BINVAL)
        self.checkList(xattr.list(fname), [self.USER_ATTR])
        self.assertEqual(xattr.list(fname, namespace=NS_USER), [self.USER_NN])
        self.assertEqual(xattr.get(fname, self.USER_ATTR), BINVAL)
        self.assertEqual(xattr.get(fname, self.USER_NN,
                                   namespace=NS_USER), BINVAL)
        self.checkTuples(xattr.get_all(fname), [(self.USER_ATTR, BINVAL)])
        self.assertEqual(xattr.get_all(fname, namespace=NS_USER),
                         [(self.USER_NN, BINVAL)])
        xattr.remove(fname, self.USER_ATTR)

    def testManyOpsDeprecated(self):
        """test many ops (deprecated functions)"""
        fh, fname = self._getfile()
        xattr.setxattr(fh, self.USER_ATTR, self.USER_VAL)
        VL = [self.USER_ATTR]
        for i in range(self.MANYOPS_COUNT):
            self.checkList(xattr.listxattr(fh), VL)
        for i in range(self.MANYOPS_COUNT):
            self.assertEqual(xattr.getxattr(fh, self.USER_ATTR), self.USER_VAL)
        for i in range(self.MANYOPS_COUNT):
            self.checkTuples(xattr.get_all(fh),
                             [(self.USER_ATTR, self.USER_VAL)])

    def testManyOps(self):
        """test many ops"""
        fh, fname = self._getfile()
        xattr.set(fh, self.USER_ATTR, self.USER_VAL)
        VL = [self.USER_ATTR]
        VN = [self.USER_NN]
        for i in range(self.MANYOPS_COUNT):
            self.checkList(xattr.list(fh), VL)
            self.checkList(xattr.list(fh, namespace=EMPTY_NS), VL)
            self.assertEqual(xattr.list(fh, namespace=NS_USER), VN)
        for i in range(self.MANYOPS_COUNT):
            self.assertEqual(xattr.get(fh, self.USER_ATTR), self.USER_VAL)
            self.assertEqual(xattr.get(fh, self.USER_NN, namespace=NS_USER),
                             self.USER_VAL)
        for i in range(self.MANYOPS_COUNT):
            self.checkTuples(xattr.get_all(fh),
                             [(self.USER_ATTR, self.USER_VAL)])
            self.assertEqual(xattr.get_all(fh, namespace=NS_USER),
                             [(self.USER_NN, self.USER_VAL)])

    def testNoneNamespace(self):
        fh, fname = self._getfile()
        self.assertRaises(TypeError, xattr.get, fh, self.USER_ATTR,
                          namespace=None)


if __name__ == "__main__":
    unittest.main()
