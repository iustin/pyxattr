# pyxattr

This is the pyxattr module, a Python extension module which gives access
to the extended attributes for filesystem objects available in some
operating systems.

[![Travis](https://img.shields.io/travis/iustin/pyxattr)](https://travis-ci.org/iustin/pyxattr)
[![Codecov](https://img.shields.io/codecov/c/github/iustin/pyxattr)](https://codecov.io/gh/iustin/pyxattr)
[![Read the Docs](https://img.shields.io/readthedocs/pyxattr)](http://pyxattr.readthedocs.io/en/latest/?badge=latest)
[![GitHub issues](https://img.shields.io/github/issues/iustin/pyxattr)](https://github.com/iustin/pyxattr/issues)
![GitHub tag (latest by date)](https://img.shields.io/github/v/tag/iustin/pyxattr)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/iustin/pyxattr)](https://github.com/iustin/pyxattr/releases)
[![PyPI](https://img.shields.io/pypi/v/pyxattr)](https://pypi.org/project/pyxattr/)
![Debian package](https://img.shields.io/debian/v/python-pyxattr)
![Ubuntu package](https://img.shields.io/ubuntu/v/python-pyxattr)
![GitHub Release Date](https://img.shields.io/github/release-date/iustin/pyxattr)
![GitHub commits since latest release](https://img.shields.io/github/commits-since/iustin/pyxattr/latest)
![GitHub last commit](https://img.shields.io/github/last-commit/iustin/pyxattr)

Downloads: go to <https://pyxattr.k1024.org/downloads/>. The source
repository is either at <http://git.k1024.org/pyxattr.git> or at
<https://github.com/iustin/pyxattr>.

## Requirements

The current supported Python versions are 3.4+ (tested up to 3.9).

The library has been written and tested on Linux, kernel v2.4 or
later, with XFS and ext2/ext3/ext3 file systems. If any other platform
implements the same behaviour, pyxattr could be used.

You need to have the setuptools tool installed in order to build and
install the module, and for building the documentation you need to
have Sphinx installed.

Alternatively, you can install directly from pip:

    $ pip install pyxattr

Or from your distribution, e.g. in Debian:

    $ sudo apt install python3-pyxattr

## Basic example

    >>> import xattr
    >>> xattr.listxattr("file.txt")
    ['user.mime_type']
    >>> xattr.getxattr("file.txt", "user.mime_type")
    'text/plain'
    >>> xattr.setxattr("file.txt", "user.comment", "Simple text file")
    >>> xattr.listxattr("file.txt")
    ['user.mime_type', 'user.comment']
    >>> xattr.removexattr ("file.txt", "user.comment")

## License

pyxattr is Copyright 2002-2008, 2012-2015 Iustin Pop.

pyxattr is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 2.1 of the License, or (at your option) any
later version. See the COPYING file for the full license terms.

Note that previous versions had different licenses: version 0.3 was licensed
under LGPL version 3 (which, I realized later, is not compatible with GPLv2,
hence the change to LGPL 2.1), and even older versions were licensed under GPL
v2 or later.
