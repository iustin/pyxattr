#!/usr/bin/env python

import distutils
from distutils.core import setup, Extension

long_desc = """This is a C extension module for Python which
implements extended attributes manipulation. It is a wrapper on top
of the attr C library - see attr(5)."""
version = "0.2"

setup(name="pyxattr",
      version=version,
      description="Extended attributes for python",
      long_description=long_desc,
      author="Iustin Pop",
      author_email="iusty@k1024.org",
      url="http://pyxattr.sourceforge.net",
      license="GPL",
      ext_modules=[Extension("xattr", ["xattr.c"], libraries=["attr"])],
      data_files=[("/usr/share/doc/pyxattr-%s" % version,
                  ["README", "xattr.html", "xattr.txt"])]
      )
