#!/usr/bin/env python2

import distutils
from distutils.core import setup, Extension

setup(name="pyxattr",
      version="0.1",
      ext_modules=[Extension("xattr", ["xattr.c"], libraries=["attr"])],
      )
