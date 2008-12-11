#!/bin/sh

set -e

for ver in 2.4 2.5 2.6 3.0; do
    rm -f xattr.so
    python$ver ./setup.py build_ext -i
    PYTHONPATH=. python$ver ./test/test_xattr.py
done
