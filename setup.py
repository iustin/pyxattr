#!/usr/bin/env python2

import distutils
from distutils.core import setup, Extension

setup(name="pyxattr",
      version="0.1",
      description="Extended attributes for python",
      long_description="""This is a C extension module for Python which
      implements extended attributes manipulation. It is a wrapper on top
      of the attr C library - see attr(5).""",
      author="Iustin Pop",
      author_email="iusty@k1024.org",
      url="http://pyxattr.sourceforge.net",
      license="GPL",
      ext_modules=[Extension("xattr", ["xattr.c"], libraries=["attr"])],
      )
