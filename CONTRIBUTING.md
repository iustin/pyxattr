# Contributing to pyxattr

Hi, and thanks for any and all contributions!

## Bugs and patches

This is a small project, so let's keep things simple:

- Please file all bug reports on github
  (<https://github.com/iustin/pyxattr/issues>), as this allows
  archival and discovery by other people;
- Send patches as pull requests; for larger changes, would be good to
  first open a bug to discuss the plans;

Due to simplicity, there are no old branches being kept alive, but if
it ever happens that a bug is found in older versions and there is
needed to support older Python versions, it is possible to do so.

## Code standards

There are no formal standards, but:

- Code should be tested - this is why there's a [Codecov
  integration](https://app.codecov.io/gh/iustin/pyxattr/tree/main).
- New functions should have good docstrings (in the C code).
- New functions/constants should be listed in the documentation, see
  `doc/module.rst` for how to include them.
- All non-trivial changes should be listed in `NEWS.md` for further
  inclusion in new releases documentation. Add an "unreleased" section
  (if one doesn't exist yet) to list the changes.

## Release process

Right now, due to GPG signing, I'm doing releases and signing them
manually (offline, I mean). Basically, once GitHub workflows are fine:

- Bump the version in all places - use `git grep -F $OLD_VER` and
  update as needed.
- Ensure that `setup.py` has the right Python versions listed (bit me
  more than once).
- Update the `NEWS.md` file is up to date (contents), and use the
  right date.
- Check that the generated documentation (`make doc`) looks right.

Then run these steps:

```
$ make clean
$ make distcheck # this leaves things in dist/
$ git tag -m 'Release pyxattr-0.0.1' --sign v0.0.1
$ gpg --sign -b -a dist/pyxattr-0.0.1.tar.gz
$ python3 -m twine upload dist/*
```

Separately:

* Upload the `dist/` contents to GitHub and tag a new release.
* Upload the `dist/` contents to the old-style download area,
  <https://pyxattr.k1024.org/downloads/>.

Hopefully one day all this can be more automated.

## Signing key

The releases are currently signed by my key, see <https://k1024.org/contact/>.
