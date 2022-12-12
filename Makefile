PYTHON        = python3
SPHINXOPTS    = -W
SPHINXBUILD   = $(PYTHON) -m sphinx
DOCDIR        = doc
DOCHTML       = $(DOCDIR)/html
DOCTREES      = $(DOCDIR)/doctrees
ALLSPHINXOPTS = -d $(DOCTREES) $(SPHINXOPTS) $(DOCDIR)
VERSION       = 0.8.0
FULLVER       = pyxattr-$(VERSION)
DISTFILE      = $(FULLVER).tar.gz

MODNAME = xattr.so
RSTFILES = doc/index.rst doc/module.rst doc/news.rst doc/readme.md doc/conf.py
PYVERS = 3.7 3.8 3.9 3.10
REPS = 5

all: doc test

$(MODNAME): xattr.c
	$(PYTHON) ./setup.py build_ext --inplace

$(DOCHTML)/index.html: $(MODNAME) $(RSTFILES)
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) $(DOCHTML)
	touch $@

doc: $(DOCHTML)/index.html

dist:
	fakeroot $(PYTHON) ./setup.py sdist

distcheck: dist
	set -e; \
	TDIR=$$(mktemp -d) && \
	trap "rm -rf $$TDIR" EXIT; \
	tar xzf dist/$(DISTFILE) -C $$TDIR && \
	(cd $$TDIR/$(FULLVER) && make doc && make test && make dist) && \
	echo "All good, you can upload $(DISTFILE)!"

test:
	@for ver in $(PYVERS); do \
	  for flavour in "" "-dbg"; do \
	    if type python$$ver$$flavour >/dev/null; then \
	      echo Testing with python$$ver$$flavour; \
	      python$$ver$$flavour setup.py build_ext -i; \
	      python$$ver$$flavour -m pytest tests; \
	    fi; \
	  done; \
	done;
	@if type pypy3 >/dev/null; then \
	  echo Testing with pypy3; \
	  pypy3 setup.py build_ext -i; \
	  pypy3 -m pytest tests; \
	fi

fast-test:
	python3 setup.py build_ext -i
	python3 -m pytest tests -v

benchmark: $(MODNAME)
	@set -e; \
	TESTFILE=`mktemp`;\
	trap 'rm $$TESTFILE' EXIT; \
	for ver in $(PYVERS) ; do \
	    if type python$$ver >/dev/null; then \
	      echo Benchmarking with python$$ver; \
	      python$$ver ./setup.py build -q; \
	      echo "  - set (with override)"; \
	      python$$ver -m timeit -r $(REPS) -s 'import xattr' "xattr.set('$$TESTFILE', 'user.comment', 'hello')"; \
	      echo "  - list"; \
	      python$$ver -m timeit -r $(REPS) -s 'import xattr' "xattr.list('$$TESTFILE')"; \
	      echo "  - get"; \
	      python$$ver -m timeit -r $(REPS) -s 'import xattr' "xattr.get('$$TESTFILE', 'user.comment')"; \
	      echo "  - set + remove"; \
	      python$$ver -m timeit -r $(REPS) -s 'import xattr' "xattr.set('$$TESTFILE', 'user.comment', 'hello'); xattr.remove('$$TESTFILE', 'user.comment')"; \
	    fi; \
	done;

coverage:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-coverage"
	lcov --capture --directory . --no-external --output-file coverage.info
	genhtml coverage.info --output-directory out

clean:
	rm -rf $(DOCHTML) $(DOCTREES)
	rm -f $(MODNAME)
	rm -f *.so
	rm -rf build

.PHONY: doc test fast-test clean dist distcheck coverage
