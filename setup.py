#!/usr/bin/env python3

import distutils
import platform
try:
  from setuptools import setup, Extension
except ImportError:
  from distutils.core import setup, Extension

long_desc = """This is a C extension module for Python which
implements extended attributes manipulation. It is a wrapper on top
of the attr C library - see attr(5)."""
version = "0.7.2"
author = "Iustin Pop"
author_email = "iustin@k1024.org"
libraries = []
macros = [
    ("_XATTR_VERSION", '"%s"' % version),
    ("_XATTR_AUTHOR", '"%s"' % author),
    ("_XATTR_EMAIL", '"%s"' % author_email),
    ]
setup(name = "pyxattr",
      version = version,
      description = "Filesystem extended attributes for python",
      long_description = long_desc,
      author = author,
      author_email = author_email,
      url = "http://pyxattr.k1024.org/",
      download_url = "http://pyxattr.k1024.org/downloads/",
      license = "LGPL",
      ext_modules = [Extension("xattr", ["xattr.c"],
                               libraries=libraries,
                               define_macros=macros,
                               extra_compile_args=["-Wall", "-Werror", "-Wsign-compare"],
                               )],
      platforms = ["Linux"],
      python_requires = ">=3.4",
      project_urls={
        "Bug Tracker": "https://github.com/iustin/pyxattr/issues",
      },
      classifiers = [
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Lesser General Public License v2 or later (LGPLv2+)",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: Implementation :: PyPy",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: POSIX :: Linux",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: System :: Filesystems",
      ]
      )
