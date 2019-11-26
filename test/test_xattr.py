#
#

import sys
import tempfile
import os
import errno
import pytest
import pathlib
import platform
import io

import xattr
from xattr import NS_USER, XATTR_CREATE, XATTR_REPLACE

NAMESPACE = os.environ.get("NAMESPACE", NS_USER)

EMPTY_NS = bytes()

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
MANYOPS_COUNT = 16384

USER_NN = USER_NN.encode()
USER_VAL = USER_VAL.encode()
USER_ATTR = USER_ATTR.encode()
EMPTY_VAL = EMPTY_VAL.encode()
LARGE_VAL = LARGE_VAL.encode()

# Helper functions

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

# Fixtures and helpers

@pytest.fixture
def testdir():
    """per-test temp dir based in TEST_DIR"""
    with tempfile.TemporaryDirectory(dir=TEST_DIR) as dname:
        yield dname

def get_file(path):
    fh, fname = tempfile.mkstemp(".test", "xattr-", path)
    return fh, fname

def get_file_name(path):
    fh, fname = get_file(path)
    os.close(fh)
    return fname

def get_file_fd(path):
    return get_file(path)[0]

def get_file_object(path):
    fd = get_file(path)[0]
    return os.fdopen(fd)

def get_dir(path):
    return tempfile.mkdtemp(".test", "xattr-", path)

def get_symlink(path, dangling=True):
    """create a symlink"""
    fh, fname = get_file(path)
    os.close(fh)
    if dangling:
        os.unlink(fname)
    sname = fname + ".symlink"
    os.symlink(fname, sname)
    return fname, sname

def get_valid_symlink(path):
    return get_symlink(path, dangling=False)[1]

def get_dangling_symlink(path):
    return get_symlink(path, dangling=True)[1]

def as_bytes(call):
    def f(path):
        return call(path).encode()
    return f

def as_fspath(call):
    def f(path):
        return pathlib.PurePath(call(path))
    return f

def as_iostream(call):
    def f(path):
        return io.open(call(path), "r")
    return f

NOT_BEFORE_36 = pytest.mark.xfail(condition="sys.version_info < (3,6)",
                                  strict=True)
NOT_PYPY = pytest.mark.xfail(condition="platform.python_implementation() == 'PyPy'",
                                  strict=False)

# Note: user attributes are only allowed on files and directories, so
# we have to skip the symlinks here. See xattr(7).
ITEMS_P = [
    (get_file_name, False),
    (as_bytes(get_file_name), False),
    pytest.param((as_fspath(get_file_name), False),
                 marks=[NOT_BEFORE_36, NOT_PYPY]),
    (get_file_fd, False),
    (get_file_object, False),
    (as_iostream(get_file_name), False),
    (get_dir, False),
    (as_bytes(get_dir), False),
    pytest.param((as_fspath(get_dir), False),
                 marks=[NOT_BEFORE_36, NOT_PYPY]),
    (get_valid_symlink, False),
    (as_bytes(get_valid_symlink), False),
    pytest.param((as_fspath(get_valid_symlink), False),
                 marks=[NOT_BEFORE_36, NOT_PYPY]),
]

ITEMS_D = [
    "file name",
    "file name (bytes)",
    "file name (path)",
    "file FD",
    "file object",
    "file io stream",
    "directory",
    "directory (bytes)",
    "directory (path)",
    "file via symlink",
    "file via symlink (bytes)",
    "file via symlink (path)",
]

ALL_ITEMS_P = ITEMS_P + [
    (get_valid_symlink, True),
    (as_bytes(get_valid_symlink), True),
    (get_dangling_symlink, True),
    (as_bytes(get_dangling_symlink), True),
]

ALL_ITEMS_D = ITEMS_D + [
    "valid symlink",
    "valid symlink (bytes)",
    "dangling symlink",
    "dangling symlink (bytes)"
]

@pytest.fixture(params=ITEMS_P, ids=ITEMS_D)
def subject(testdir, request):
    return request.param[0](testdir), request.param[1]

@pytest.fixture(params=ALL_ITEMS_P, ids=ALL_ITEMS_D)
def any_subject(testdir, request):
    return request.param[0](testdir), request.param[1]

@pytest.fixture(params=[True, False], ids=["with namespace", "no namespace"])
def use_ns(request):
    return request.param

@pytest.fixture(params=[True, False], ids=["dangling", "valid"])
def use_dangling(request):
    return request.param

### Test functions

def test_empty_value(subject):
    item, nofollow = subject
    xattr.set(item, USER_ATTR, EMPTY_VAL, nofollow=nofollow)
    assert xattr.get(item, USER_ATTR, nofollow=nofollow) == EMPTY_VAL

def test_large_value(subject):
    item, nofollow = subject
    xattr.set(item, USER_ATTR, LARGE_VAL)
    assert xattr.get(item, USER_ATTR, nofollow=nofollow) == LARGE_VAL


def test_file_mixed_access_deprecated(testdir):
    """test mixed access to file (deprecated functions)"""
    fh, fname = get_file(testdir)
    with os.fdopen(fh) as fo:
        lists_equal(xattr.listxattr(fname), [])
        xattr.setxattr(fname, USER_ATTR, USER_VAL)
        lists_equal(xattr.listxattr(fh), [USER_ATTR])
        assert xattr.getxattr(fo, USER_ATTR) == USER_VAL
        tuples_equal(xattr.get_all(fo), [(USER_ATTR, USER_VAL)])
        tuples_equal(xattr.get_all(fname),
                     [(USER_ATTR, USER_VAL)])

def test_file_mixed_access(testdir):
    """test mixed access to file"""
    fh, fname = get_file(testdir)
    with os.fdopen(fh) as fo:
        lists_equal(xattr.list(fname), [])
        xattr.set(fname, USER_ATTR, USER_VAL)
        lists_equal(xattr.list(fh), [USER_ATTR])
        assert xattr.list(fh, namespace=NAMESPACE) == [USER_NN]
        assert xattr.get(fo, USER_ATTR) == USER_VAL
        assert xattr.get(fo, USER_NN, namespace=NAMESPACE) == USER_VAL
        tuples_equal(xattr.get_all(fo),
                     [(USER_ATTR, USER_VAL)])
        assert xattr.get_all(fo, namespace=NAMESPACE) == \
            [(USER_NN, USER_VAL)]
        tuples_equal(xattr.get_all(fname), [(USER_ATTR, USER_VAL)])
        assert xattr.get_all(fname, namespace=NAMESPACE) == \
            [(USER_NN, USER_VAL)]

def test_replace_on_missing(subject, use_ns):
    item = subject[0]
    lists_equal(xattr.list(item), [])
    with pytest.raises(EnvironmentError):
        if use_ns:
            xattr.set(item, USER_NN, USER_VAL, flags=XATTR_REPLACE,
                      namespace=NAMESPACE)
        else:
            xattr.set(item, USER_ATTR, USER_VAL, flags=XATTR_REPLACE)

def test_create_on_existing(subject, use_ns):
    item = subject[0]
    lists_equal(xattr.list(item), [])
    if use_ns:
        xattr.set(item, USER_NN, USER_VAL,
                  namespace=NAMESPACE)
    else:
        xattr.set(item, USER_ATTR, USER_VAL)
    with pytest.raises(EnvironmentError):
        if use_ns:
            xattr.set(item, USER_NN, USER_VAL,
                      flags=XATTR_CREATE, namespace=NAMESPACE)
        else:
            xattr.set(item, USER_ATTR, USER_VAL, flags=XATTR_CREATE)

def test_remove_on_missing(any_subject, use_ns):
    item, nofollow = any_subject
    lists_equal(xattr.list(item, nofollow=nofollow), [])
    with pytest.raises(EnvironmentError):
        if use_ns:
            xattr.remove(item, USER_NN, namespace=NAMESPACE,
                         nofollow=nofollow)
        else:
            xattr.remove(item, USER_ATTR, nofollow=nofollow)

def test_set_get_remove(subject, use_ns):
    item = subject[0]
    lists_equal(xattr.list(item), [])
    if use_ns:
        xattr.set(item, USER_NN, USER_VAL,
                  namespace=NAMESPACE)
    else:
        xattr.set(item, USER_ATTR, USER_VAL)
    if use_ns:
        assert xattr.list(item, namespace=NAMESPACE) == [USER_NN]
    else:
        lists_equal(xattr.list(item), [USER_ATTR])
        lists_equal(xattr.list(item, namespace=EMPTY_NS),
                    [USER_ATTR])
    if use_ns:
        assert xattr.get(item, USER_NN, namespace=NAMESPACE) == USER_VAL
    else:
        assert xattr.get(item, USER_ATTR) == USER_VAL
    if use_ns:
        assert xattr.get_all(item, namespace=NAMESPACE) == \
            [(USER_NN, USER_VAL)]
    else:
        tuples_equal(xattr.get_all(item),
                     [(USER_ATTR, USER_VAL)])
    if use_ns:
        xattr.remove(item, USER_NN, namespace=NAMESPACE)
    else:
        xattr.remove(item, USER_ATTR)
    lists_equal(xattr.list(item), [])
    tuples_equal(xattr.get_all(item), [])

def test_replace_on_missing_deprecated(subject):
    item = subject[0]
    lists_equal(xattr.listxattr(item), [])
    with pytest.raises(EnvironmentError):
        xattr.setxattr(item, USER_ATTR, USER_VAL, XATTR_REPLACE)

def test_create_on_existing_deprecated(subject):
    item = subject[0]
    lists_equal(xattr.listxattr(item), [])
    xattr.setxattr(item, USER_ATTR, USER_VAL, 0)
    with pytest.raises(EnvironmentError):
        xattr.setxattr(item, USER_ATTR, USER_VAL, XATTR_CREATE)

def test_remove_on_missing_deprecated(any_subject):
    """check deprecated list, set, get operations against an item"""
    item, nofollow = any_subject
    lists_equal(xattr.listxattr(item, nofollow), [])
    with pytest.raises(EnvironmentError):
        xattr.removexattr(item, USER_ATTR)

def test_set_get_remove_deprecated(subject):
    """check deprecated list, set, get operations against an item"""
    item = subject[0]
    lists_equal(xattr.listxattr(item), [])
    xattr.setxattr(item, USER_ATTR, USER_VAL, 0)
    lists_equal(xattr.listxattr(item), [USER_ATTR])
    assert xattr.getxattr(item, USER_ATTR) == USER_VAL
    tuples_equal(xattr.get_all(item), [(USER_ATTR, USER_VAL)])
    xattr.removexattr(item, USER_ATTR)
    lists_equal(xattr.listxattr(item), [])
    tuples_equal(xattr.get_all(item), [])

def test_many_ops(subject):
    """test many ops"""
    item = subject[0]
    xattr.set(item, USER_ATTR, USER_VAL)
    VL = [USER_ATTR]
    VN = [USER_NN]
    for i in range(MANYOPS_COUNT):
        lists_equal(xattr.list(item), VL)
        lists_equal(xattr.list(item, namespace=EMPTY_NS), VL)
        assert xattr.list(item, namespace=NAMESPACE) == VN
    for i in range(MANYOPS_COUNT):
        assert xattr.get(item, USER_ATTR) == USER_VAL
        assert xattr.get(item, USER_NN, namespace=NAMESPACE) == USER_VAL
    for i in range(MANYOPS_COUNT):
        tuples_equal(xattr.get_all(item),
                     [(USER_ATTR, USER_VAL)])
        assert xattr.get_all(item, namespace=NAMESPACE) == \
            [(USER_NN, USER_VAL)]

def test_many_ops_deprecated(subject):
    """test many ops (deprecated functions)"""
    item = subject[0]
    xattr.setxattr(item, USER_ATTR, USER_VAL)
    VL = [USER_ATTR]
    for i in range(MANYOPS_COUNT):
        lists_equal(xattr.listxattr(item), VL)
    for i in range(MANYOPS_COUNT):
        assert xattr.getxattr(item, USER_ATTR) == USER_VAL
    for i in range(MANYOPS_COUNT):
        tuples_equal(xattr.get_all(item),
                     [(USER_ATTR, USER_VAL)])

def test_no_attributes_deprecated(any_subject):
    """test no attributes (deprecated functions)"""
    item, nofollow = any_subject
    lists_equal(xattr.listxattr(item, True), [])
    tuples_equal(xattr.get_all(item, True), [])
    with pytest.raises(EnvironmentError):
        xattr.getxattr(item, USER_ATTR, True)

def test_no_attributes(any_subject):
    """test no attributes"""
    item, nofollow = any_subject
    lists_equal(xattr.list(item, nofollow=nofollow), [])
    assert xattr.list(item, nofollow=nofollow,
                      namespace=NAMESPACE) == []
    tuples_equal(xattr.get_all(item, nofollow=nofollow), [])
    assert xattr.get_all(item, nofollow=nofollow,
                         namespace=NAMESPACE) == []
    with pytest.raises(EnvironmentError):
        xattr.get(item, USER_NN, nofollow=nofollow,
                  namespace=NAMESPACE)

def test_binary_payload_deprecated(subject):
    """test binary values (deprecated functions)"""
    item = subject[0]
    BINVAL = b"abc\0def"
    xattr.setxattr(item, USER_ATTR, BINVAL)
    lists_equal(xattr.listxattr(item), [USER_ATTR])
    assert xattr.getxattr(item, USER_ATTR) == BINVAL
    tuples_equal(xattr.get_all(item), [(USER_ATTR, BINVAL)])
    xattr.removexattr(item, USER_ATTR)

def test_binary_payload(subject):
    """test binary values"""
    item = subject[0]
    BINVAL = b"abc\0def"
    xattr.set(item, USER_ATTR, BINVAL)
    lists_equal(xattr.list(item), [USER_ATTR])
    assert xattr.list(item, namespace=NAMESPACE) == [USER_NN]
    assert xattr.get(item, USER_ATTR) == BINVAL
    assert xattr.get(item, USER_NN, namespace=NAMESPACE) == BINVAL
    tuples_equal(xattr.get_all(item), [(USER_ATTR, BINVAL)])
    assert xattr.get_all(item, namespace=NAMESPACE) == [(USER_NN, BINVAL)]
    xattr.remove(item, USER_ATTR)

def test_symlinks_user_fail(testdir, use_dangling):
    _, sname = get_symlink(testdir, dangling=use_dangling)
    with pytest.raises(IOError):
        xattr.set(sname, USER_ATTR, USER_VAL, nofollow=True)
    with pytest.raises(IOError):
        xattr.set(sname, USER_NN, USER_VAL, namespace=NAMESPACE,
                  nofollow=True)
    with pytest.raises(IOError):
        xattr.setxattr(sname, USER_ATTR, USER_VAL, XATTR_CREATE, True)

@pytest.mark.parametrize(
    "call, args", [(xattr.get, [USER_ATTR]),
                   (xattr.list, []),
                   (xattr.remove, [USER_ATTR]),
                   (xattr.get, [USER_ATTR]),
                   (xattr.set, [USER_ATTR, USER_VAL])])
def test_none_namespace(testdir, call, args):
    # Don't want to use subject, since that would prevent xfail test
    # on path objects (due to hiding the exception here).
    f = get_file_name(testdir)
    with pytest.raises(TypeError):
        call(f, *args, namespace=None)
    fd = get_file_fd(testdir)
    with pytest.raises(TypeError):
        call(fd, *args, namespace=None)

@pytest.mark.parametrize(
    "call",
    [xattr.get, xattr.list, xattr.listxattr,
     xattr.remove, xattr.removexattr,
     xattr.set, xattr.setxattr,
     xattr.get, xattr.getxattr])
def test_wrong_call(call):
    with pytest.raises(TypeError):
        call()

@pytest.mark.parametrize(
    "call, args", [(xattr.get, [USER_ATTR]),
                   (xattr.listxattr, []),
                   (xattr.list, []),
                   (xattr.remove, [USER_ATTR]),
                   (xattr.removexattr, [USER_ATTR]),
                   (xattr.get, [USER_ATTR]),
                   (xattr.getxattr, [USER_ATTR]),
                   (xattr.set, [USER_ATTR, USER_VAL]),
                   (xattr.setxattr, [USER_ATTR, USER_VAL])])
def test_wrong_argument_type(call, args):
    with pytest.raises(TypeError):
        call(object(), *args)
