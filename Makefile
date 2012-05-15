SPHINXOPTS    = -W
SPHINXBUILD   = sphinx-build
DOCDIR        = doc
DOCHTML       = $(DOCDIR)/html
DOCTREES      = $(DOCDIR)/doctrees
ALLSPHINXOPTS = -d $(DOCTREES) $(SPHINXOPTS) $(DOCDIR)

MODNAME = xattr.so
RSTFILES = doc/index.rst doc/module.rst NEWS README doc/conf.py

all: doc test

$(MODNAME): xattr.c
	./setup.py build_ext --inplace

$(DOCHTML)/index.html: $(MODNAME) $(RSTFILES)
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) $(DOCHTML)
	touch $@

doc: $(DOCHTML)/index.html

dist:
	fakeroot ./setup.py sdist

test:
	for ver in 2.4 2.5 2.6 2.7 3.0 3.1 3.2; do \
	  if type python$$ver >/dev/null; then \
	    echo Testing with python$$ver; \
	    python$$ver ./setup.py test; \
          fi; \
	done

clean:
	rm -rf $(DOCHTML) $(DOCTREES)
	rm -f $(MODNAME)
	rm -rf build

.PHONY: doc test clean dist
