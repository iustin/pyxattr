doc: xattr.txt xattr.html

build/lib.linux-i686-2.3/xattr.so: xattr.c
	./setup.py build

xattr.txt: build/lib.linux-i686-2.3/xattr.so
	PYTHONPATH=build/lib.linux-i686-2.3 pydoc xattr > xattr.txt

xattr.html: build/lib.linux-i686-2.3/xattr.so
	PYTHONPATH=build/lib.linux-i686-2.3 pydoc -w xattr

clean:
	rm -f xattr.txt xattr.html
	rm -rf build dist

.PHONY: log clean
