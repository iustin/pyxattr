#!/usr/bin/env python2

import distutils
from distutils.core import setup, Extension

setup(name="pyacl",
      version="0.1",
      ext_modules=[Extension("attrmodule", ["attr.c"], libraries=["attr"])],
      )
