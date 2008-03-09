#!/usr/bin/python

#import distutils
#from distutils.core import setup, Extension
from setuptools import setup, Extension

long_desc = """This is a C extension module for Python which
implements extended attributes manipulation. It is a wrapper on top
of the attr C library - see attr(5)."""
version = "0.3.0"

setup(name = "pyxattr",
      version = version,
      description = "Filesystem extended attributes for python",
      long_description = long_desc,
      author = "Iustin Pop",
      author_email = "iusty@k1024.org",
      url = "http://pyxattr.sourceforge.net",
      license = "LGPL",
      ext_modules = [Extension("xattr", ["xattr.c"], libraries=["attr"])],
      test_suite = "test/test_xattr",
      )
