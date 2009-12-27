#!/bin/sh

set -e

for ver in 2.4 2.5 2.6 3.0 3.1; do
    if ! type python$ver >/dev/null; then
        echo Skipping python$ver
        continue
    fi
    echo Testing using python$ver
    rm -f xattr.so
    python$ver ./setup.py clean
    python$ver ./setup.py build_ext -i
    PYTHONPATH=. python$ver ./test/test_xattr.py
done
