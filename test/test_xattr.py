#
#

import unittest
import tempfile
import os
import errno

import xattr

class xattrTest(unittest.TestCase):
    USER_ATTR = "user.test"
    USER_VAL = "abc"

    def setUp(self):
        """set up function"""
        self.rmfiles = []
        self.rmdirs = []

    def tearDown(self):
        """tear down function"""
        for fname in self.rmfiles:
            os.unlink(fname)
        for dname in self.rmdirs:
            os.rmdir(dname)

    def _getfile(self):
        """create a temp file"""
        fh, fname = tempfile.mkstemp(".test", "xattr-", ".")
        self.rmfiles.append(fname)
        return fh, fname

    def _getdir(self):
        """create a temp dir"""
        dname = tempfile.mkdtemp(".test", "xattr-", ".")
        self.rmdirs.append(dname)
        return dname

    def _getsymlink(self):
        """create a symlink"""
        fh, fname = self._getfile()
        os.close(fh)
        os.unlink(fname)
        os.symlink(fname + ".non-existent", fname)
        return fname

    def _checkListSetGet(self, item, symlink=False):
        """check list, set, get operations against an item"""
        self.failUnlessEqual(xattr.listxattr(item, symlink), [])
        self.failUnlessRaises(EnvironmentError, xattr.setxattr, item,
                              self.USER_ATTR, self.USER_VAL,
                              xattr.XATTR_REPLACE)
        try:
            xattr.setxattr(item, self.USER_ATTR, self.USER_VAL, 0, symlink)
        except IOError, err:
            if err.errno == errno.EPERM and symlink:
                # symlinks may fail, in which case we abort the rest
                # of the test for this case
                return
            raise
        self.failUnlessRaises(EnvironmentError, xattr.setxattr, item,
                              self.USER_ATTR, self.USER_VAL,
                              xattr.XATTR_CREATE)
        self.failUnlessEqual(xattr.listxattr(item, symlink), [self.USER_ATTR])
        self.failUnlessEqual(xattr.getxattr(item, self.USER_ATTR, symlink),
                             self.USER_VAL)
        self.failUnlessEqual(xattr.get_all(item, nofollow=symlink),
                             [(self.USER_ATTR, self.USER_VAL)])
        xattr.removexattr(item, self.USER_ATTR)
        self.failUnlessEqual(xattr.listxattr(item, symlink), [])
        self.failUnlessEqual(xattr.get_all(item, nofollow=symlink), [])
        self.failUnlessRaises(EnvironmentError, xattr.removexattr,
                              item, self.USER_ATTR)

    def testNoXattr(self):
        """test no attributes"""
        fh, fname = self._getfile()
        self.failUnlessEqual(xattr.listxattr(fname), [])
        self.failUnlessEqual(xattr.get_all(fname), [])
        dname = self._getdir()
        self.failUnlessEqual(xattr.listxattr(dname), [])
        self.failUnlessEqual(xattr.get_all(dname), [])
        sname = self._getsymlink()
        self.failUnlessEqual(xattr.listxattr(sname, True), [])
        self.failUnlessEqual(xattr.get_all(sname, nofollow=True), [])

    def testFileByName(self):
        """test set and retrieve one attribute by file name"""
        fh, fname = self._getfile()
        self._checkListSetGet(fname)
        os.close(fh)

    def testFileByDescriptor(self):
        """test file descriptor operations"""
        fh, fname = self._getfile()
        self._checkListSetGet(fh)
        os.close(fh)

    def testFileByObject(self):
        """test file descriptor operations"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        self._checkListSetGet(fo)
        fo.close()

    def testMixedAccess(self):
        """test mixed access to file"""
        fh, fname = self._getfile()
        fo = os.fdopen(fh)
        self.failUnlessEqual(xattr.listxattr(fname), [])
        xattr.setxattr(fname, self.USER_ATTR, self.USER_VAL)
        self.failUnlessEqual(xattr.listxattr(fh), [self.USER_ATTR])
        self.failUnlessEqual(xattr.getxattr(fo, self.USER_ATTR),
                             self.USER_VAL)
        self.failUnlessEqual(xattr.get_all(fo),
                             [(self.USER_ATTR, self.USER_VAL)])
        self.failUnlessEqual(xattr.get_all(fname),
                             [(self.USER_ATTR, self.USER_VAL)])

    def testDirOps(self):
        """test attribute setting on directories"""
        dname = self._getdir()
        self._checkListSetGet(dname)

    def testSymlinkOps(self):
        """test symlink operations"""
        sname = self._getsymlink()
        self.failUnlessRaises(EnvironmentError, xattr.listxattr, sname)
        self._checkListSetGet(sname, symlink=True)

    def testBinaryPayload(self):
        """test binary values"""
        fh, fname = self._getfile()
        os.close(fh)
        BINVAL = "abc" + '\0' + "def"
        xattr.setxattr(fname, self.USER_ATTR, BINVAL)
        self.failUnlessEqual(xattr.listxattr(fname), [self.USER_ATTR])
        self.failUnlessEqual(xattr.getxattr(fname, self.USER_ATTR), BINVAL)
        self.failUnlessEqual(xattr.get_all(fname), [(self.USER_ATTR, BINVAL)])
        xattr.removexattr(fname, self.USER_ATTR)

    def testManyOps(self):
        """test many ops"""
        fh, fname = self._getfile()
        xattr.setxattr(fh, self.USER_ATTR, self.USER_VAL)
        VL = [self.USER_ATTR]
        for i in range(131072):
            self.failUnlessEqual(xattr.listxattr(fh), VL)
        for i in range(131072):
            self.failUnlessEqual(xattr.getxattr(fh, self.USER_ATTR),
                                 self.USER_VAL)
        for i in range(131072):
            self.failUnlessEqual(xattr.get_all(fh),
                                 [(self.USER_ATTR, self.USER_VAL)])
