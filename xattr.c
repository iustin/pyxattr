/*
    xattr - a python module for manipulating filesystem extended attributes

    Copyright (C) 2002, 2003, 2006, 2008, 2012, 2013, 2015
      Iustin Pop <iustin@k1024.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA

*/

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#if defined(__APPLE__)
#include <sys/xattr.h>
#elif defined(__linux__)
#include <attr/xattr.h>
#endif
#include <stdio.h>

/* Compatibility with python 2.4 regarding python size type (PEP 353) */
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#define BYTES_CHAR "y"
#define BYTES_TUPLE "yy#"
#else
#define BYTES_CHAR "s"
#define BYTES_TUPLE "ss#"
#define PyBytes_Check PyString_Check
#define PyBytes_AS_STRING PyString_AS_STRING
#define PyBytes_FromStringAndSize PyString_FromStringAndSize
#define PyBytes_FromString PyString_FromString
#endif

#define ITEM_DOC \
    ":param item: a string representing a file-name, or a file-like\n" \
    "    object, or a file descriptor; this represents the file on \n" \
    "    which to act\n"

#define NOFOLLOW_DOC \
    ":param nofollow: if true and if\n" \
    "    the file name given is a symbolic link, the\n" \
    "    function will operate on the symbolic link itself instead\n" \
    "    of its target; defaults to false\n" \
    ":type nofollow: boolean, optional\n" \

#define NS_DOC \
    ":param namespace: if given, the attribute must not contain the\n" \
    "    namespace, but instead it will be taken from this parameter\n" \
    ":type namespace: bytes\n"

#define NAME_GET_DOC \
    ":param string name: the attribute whose value to retrieve;\n" \
    "    usually in the form of ``system.posix_acl`` or ``user.mime_type``\n"

#define NAME_SET_DOC \
    ":param string name: the attribute whose value to set;\n" \
    "    usually in the form of ``system.posix_acl`` or ``user.mime_type``\n"

#define NAME_REMOVE_DOC \
    ":param string name: the attribute to remove;\n" \
    "    usually in the form of ``system.posix_acl`` or \n" \
    "    ``user.mime_type``\n"

#define VALUE_DOC \
    ":param string value: possibly with embedded NULLs; note that there\n" \
    "    are restrictions regarding the size of the value, for\n" \
    "    example, for ext2/ext3, maximum size is the block size\n" \

#define FLAGS_DOC \
    ":param flags: if 0 or omitted the attribute will be\n" \
    "    created or replaced; if :const:`XATTR_CREATE`, the attribute\n" \
    "    will be created, giving an error if it already exists;\n" \
    "    if :const:`XATTR_REPLACE`, the attribute will be replaced,\n" \
    "    giving an error if it doesn't exist;\n" \
    ":type flags: integer\n"

#define NS_CHANGED_DOC \
    ".. versionchanged:: 0.5.1\n" \
    "   The namespace argument, if passed, cannot be None anymore; to\n" \
    "   explicitly specify an empty namespace, pass an empty\n" \
    "   string (byte string under Python 3)."


/* The initial I/O buffer size for list and get operations; if the
 * actual values will be smaller than this, we save a syscall out of
 * two and allocate more memory upfront than needed, otherwise we
 * incur three syscalls (get with ENORANGE, get with 0 to compute
 * actual size, final get). The test suite is marginally faster (5%)
 * with this, so it seems worth doing.
*/
#define ESTIMATE_ATTR_SIZE 1024

typedef enum {T_FD, T_PATH, T_LINK} target_e;

typedef struct {
    target_e type;
    union {
        const char *name;
        int fd;
    };
    PyObject *tmp;
} target_t;

/* Cleans up a tgt structure */
static void free_tgt(target_t *tgt) {
    if (tgt->tmp != NULL) {
        Py_DECREF(tgt->tmp);
    }
}

/* Used for cpychecker: */
/* The checker automatically defines this preprocessor name when creating
   the custom attribute: */
#if defined(WITH_CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION_ATTRIBUTE)
   #define CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION \
__attribute__((cpychecker_negative_result_sets_exception))
 #else
   #define CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION
 #endif

static int convert_obj(PyObject *myobj, target_t *tgt, int nofollow)
  CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION;

static int merge_ns(const char *ns, const char *name,
                    const char **result, char **buf)
  CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION;


/** Converts from a string, file or int argument to what we need.
 *
 * Returns -1 on failure, 0 on success.
 */
static int convert_obj(PyObject *myobj, target_t *tgt, int nofollow) {
    int fd;
    tgt->tmp = NULL;
    if(PyBytes_Check(myobj)) {
        tgt->type = nofollow ? T_LINK : T_PATH;
        tgt->name = PyBytes_AS_STRING(myobj);
    } else if(PyUnicode_Check(myobj)) {
        tgt->type = nofollow ? T_LINK : T_PATH;
        tgt->tmp = \
            PyUnicode_AsEncodedString(myobj,
                                      Py_FileSystemDefaultEncoding,
#ifdef IS_PY3K
                                      "surrogateescape"
#else
                                      "strict"
#endif
                                      );
        if(tgt->tmp == NULL)
            return -1;
        tgt->name = PyBytes_AS_STRING(tgt->tmp);
    } else if((fd = PyObject_AsFileDescriptor(myobj)) != -1) {
        tgt->type = T_FD;
        tgt->fd = fd;
    } else {
        PyErr_SetString(PyExc_TypeError, "argument must be string or int");
        tgt->type = T_PATH;
        tgt->name = NULL;
        return -1;
    }
    return 0;
}

/* Combine a namespace string and an attribute name into a
   fully-qualified name */
static int merge_ns(const char *ns, const char *name,
                    const char **result, char **buf) {
    if(ns != NULL && *ns != '\0') {
        int cnt;
        /* The value of new_size is related to/must be kept in-sync
           with the format string below */
        size_t new_size = strlen(ns) + 1 + strlen(name) + 1;
        if((*buf = PyMem_Malloc(new_size)) == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        cnt = snprintf(*buf, new_size, "%s.%s", ns, name);
        if((size_t) cnt >= new_size || cnt < 0) {
            PyErr_SetString(PyExc_ValueError,
                            "unexpected: can't format the attribute name");
            PyMem_Free(*buf);
            return -1;
        }
        *result = *buf;
    } else {
        *buf = NULL;
        *result = name;
    }
    return 0;
}

#if defined(__APPLE__)
static inline ssize_t _listxattr(const char *path, char *namebuf, size_t size) {
    return listxattr(path, namebuf, size, 0);
}
static inline ssize_t _llistxattr(const char *path, char *namebuf, size_t size) {
    return listxattr(path, namebuf, size, XATTR_NOFOLLOW);
}
static inline ssize_t _flistxattr(int fd, char *namebuf, size_t size) {
    return flistxattr(fd, namebuf, size, 0);
}

static inline ssize_t _getxattr (const char *path, const char *name, void *value, size_t size) {
    return getxattr(path, name, value, size, 0, 0);
}
static inline ssize_t _lgetxattr (const char *path, const char *name, void *value, size_t size) {
    return getxattr(path, name, value, size, 0, XATTR_NOFOLLOW);
}
static inline ssize_t _fgetxattr (int filedes, const char *name, void *value, size_t size) {
    return fgetxattr(filedes, name, value, size, 0, 0);
}

// [fl]setxattr: Both OS X and Linux define XATTR_CREATE and XATTR_REPLACE for the last option.
static inline int _setxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
    return setxattr(path, name, value, size, 0, flags);
}
static inline int _lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
    return setxattr(path, name, value, size, 0, flags & XATTR_NOFOLLOW);
}
static inline int _fsetxattr(int filedes, const char *name, const void *value, size_t size, int flags) {
    return fsetxattr(filedes, name, value, size, 0, flags);
}

static inline int _removexattr(const char *path, const char *name) {
    return removexattr(path, name, 0);
}
static inline int _lremovexattr(const char *path, const char *name) {
    return removexattr(path, name, XATTR_NOFOLLOW);
}
static inline int _fremovexattr(int filedes, const char *name) {
    return fremovexattr(filedes, name, 0);
}

#elif defined(__linux__)
#define _listxattr(path, list, size) listxattr(path, list, size)
#define _llistxattr(path, list, size) llistxattr(path, list, size)
#define _flistxattr(fd, list, size) flistxattr(fd, list, size)

#define _getxattr(path, name, value, size)  getxattr(path, name, value, size)
#define _lgetxattr(path, name, value, size) lgetxattr(path, name, value, size)
#define _fgetxattr(fd, name, value, size)   fgetxattr(fd, name, value, size)

#define _setxattr(path, name, value, size, flags)   setxattr(path, name, value, size, flags)
#define _lsetxattr(path, name, value, size, flags)  lsetxattr(path, name, value, size, flags)
#define _fsetxattr(fd, name, value, size, flags)    fsetxattr(fd, name, value, size, flags)

#define _removexattr(path, name)    removexattr(path, name)
#define _lremovexattr(path, name)   lremovexattr(path, name)
#define _fremovexattr(fd, name)     fremovexattr(fd, name)

#endif

typedef ssize_t (*buf_getter)(target_t *tgt, const char *name,
                              void *output, size_t size);

static ssize_t _list_obj(target_t *tgt, const char *unused, void *list,
                         size_t size) {
    if(tgt->type == T_FD)
        return _flistxattr(tgt->fd, list, size);
    else if (tgt->type == T_LINK)
        return _llistxattr(tgt->name, list, size);
    else
        return _listxattr(tgt->name, list, size);
}

static ssize_t _get_obj(target_t *tgt, const char *name, void *value,
                        size_t size) {
    if(tgt->type == T_FD)
        return _fgetxattr(tgt->fd, name, value, size);
    else if (tgt->type == T_LINK)
        return _lgetxattr(tgt->name, name, value, size);
    else
        return _getxattr(tgt->name, name, value, size);
}

static int _set_obj(target_t *tgt, const char *name,
                    const void *value, size_t size, int flags) {
    if(tgt->type == T_FD)
        return _fsetxattr(tgt->fd, name, value, size, flags);
    else if (tgt->type == T_LINK)
        return _lsetxattr(tgt->name, name, value, size, flags);
    else
        return _setxattr(tgt->name, name, value, size, flags);
}

static int _remove_obj(target_t *tgt, const char *name) {
    if(tgt->type == T_FD)
        return _fremovexattr(tgt->fd, name);
    else if (tgt->type == T_LINK)
        return _lremovexattr(tgt->name, name);
    else
        return _removexattr(tgt->name, name);
}

/* Perform a get/list operation with appropriate buffer size,
 * determined dynamically.
 *
 * Arguments:
 * - getter: the function that actually does the I/O.
 * - tgt, name: passed to the getter.
 * - buffer: pointer to either an already allocated memory area (in
 *   which case size contains its current size), or NULL to
 *   allocate. In all cases (success or failure), the caller should
 *   deallocate the buffer, using PyMem_Free(). Note that if size is
 *   zero but buffer already points to allocate memory, it will be
 *   ignored/leaked.
 * - size: either size of current buffer (if non-NULL), or size for
 *   initial allocation (if non-zero), or a zero value which means
 *   auto-allocate buffer with automatically queried size. Value will
 *   be updated upon return with the current buffer size.
 * - io_errno: if non-NULL, the actual errno will be recorded here; if
 *   zero, the call was successful and the output/size/nval are valid.
 *
 * Return value: if positive or zero, buffer will contain the read
 * value. Otherwise, io_errno will contain the I/O errno, or zero
 * to signify a Python-level error. In all cases, the Python-level
 * error is set to the appropriate value.
 */
static ssize_t _generic_get(buf_getter getter, target_t *tgt,
                            const char *name,
                            char **buffer,
                            size_t *size,
                            int *io_errno)
    CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION;

static ssize_t _generic_get(buf_getter getter, target_t *tgt,
                            const char *name,
                            char **buffer,
                            size_t *size,
                            int *io_errno) {
  ssize_t res;
  /* Clear errno for now, will only set it when it fails in I/O. */
  if (io_errno != NULL) {
    *io_errno = 0;
  }

#define EXIT_IOERROR()                 \
  {                                    \
    if (io_errno != NULL) {            \
        *io_errno = errno;             \
    }                                  \
    PyErr_SetFromErrno(PyExc_IOError); \
    return -1;                         \
  }

  /* Initialize the buffer, if needed. */
  if (*size == 0 || *buffer == NULL) {
    if (*size == 0) {
      ssize_t nalloc;
      if ((nalloc = getter(tgt, name, NULL, 0)) == -1) {
        EXIT_IOERROR();
      }
      if (nalloc == 0) {
        /* Empty, so no need to retrieve it. */
        return 0;
      }
      *size = nalloc;
    }
    if((*buffer = PyMem_Malloc(*size)) == NULL) {
      PyErr_NoMemory();
      return -1;
    }
  }
  // Try to get the value, while increasing the buffer if too small.
  while((res = getter(tgt, name, *buffer, *size)) == -1) {
    if(errno == ERANGE) {
      ssize_t realloc_size_s = getter(tgt, name, NULL, 0);
      /* ERANGE + proper size _should_ not fail, but... */
      if(realloc_size_s == -1) {
        EXIT_IOERROR();
      }
      size_t realloc_size = (size_t) realloc_size_s;
      char *tmp_buf;
      if((tmp_buf = PyMem_Realloc(*buffer, realloc_size)) == NULL) {
        PyErr_NoMemory();
        return -1;
      }
      *buffer = tmp_buf;
      *size = realloc_size;
      continue;
    } else {
      /* else we're dealing with a different error, which we
         don't know how to handle nicely, so we return */
      EXIT_IOERROR();
    }
  }
  return res;
#undef EXIT_IOERROR
}

/*
   Checks if an attribute name matches an optional namespace.

   If the namespace is NULL or an empty string, it will return the
   name itself.  If the namespace is non-NULL and the name matches, it
   will return a pointer to the offset in the name after the namespace
   and the separator. If however the name doesn't match the namespace,
   it will return NULL.

*/
const char *matches_ns(const char *ns, const char *name) {
    size_t ns_size;
    if (ns == NULL || *ns == '\0')
        return name;
    ns_size = strlen(ns);

    if (strlen(name) > (ns_size+1) && !strncmp(name, ns, ns_size) &&
        name[ns_size] == '.')
        return name + ns_size + 1;
    return NULL;
}

/* Wrapper for getxattr */
static char __pygetxattr_doc__[] =
    "getxattr(item, attribute[, nofollow=False])\n"
    "Get the value of a given extended attribute (deprecated).\n"
    "\n"
    ITEM_DOC
    NAME_GET_DOC
    NOFOLLOW_DOC
    "\n"
    ".. deprecated:: 0.4\n"
    "   this function has been deprecated\n"
    "   by the :func:`get` function.\n"
    ;

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    target_t tgt;
    int nofollow = 0;
    char *attrname = NULL;
    char *buf = NULL;
    ssize_t nret;
    size_t nalloc = ESTIMATE_ATTR_SIZE;
    PyObject *res;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oet|i", &myarg, NULL, &attrname, &nofollow))
        return NULL;
    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        res = NULL;
        goto free_arg;
    }

    nret = _generic_get(_get_obj, &tgt, attrname, &buf, &nalloc, NULL);
    if (nret == -1) {
      res = NULL;
      goto free_buf;
    }
    /* Create the string which will hold the result */
    res = PyBytes_FromStringAndSize(buf, nret);

 free_buf:
    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);
    free_tgt(&tgt);
 free_arg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

/* Wrapper for getxattr */
static char __get_doc__[] =
    "get(item, name[, nofollow=False, namespace=None])\n"
    "Get the value of a given extended attribute.\n"
    "\n"
    "Example:\n"
    "    >>> xattr.get('/path/to/file', 'user.comment')\n"
    "    'test'\n"
    "    >>> xattr.get('/path/to/file', 'comment', namespace=xattr.NS_USER)\n"
    "    'test'\n"
    "\n"
    ITEM_DOC
    NAME_GET_DOC
    NOFOLLOW_DOC
    NS_DOC
    ":return: the value of the extended attribute (can contain NULLs)\n"
    ":rtype: string\n"
    ":raises EnvironmentError: caused by any system errors\n"
    "\n"
    ".. versionadded:: 0.4\n"
    NS_CHANGED_DOC
    ;

static PyObject *
xattr_get(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg;
    target_t tgt;
    int nofollow = 0;
    char *attrname = NULL, *namebuf;
    const char *fullname;
    char *buf = NULL;
    const char *ns = NULL;
    ssize_t nret;
    size_t nalloc = ESTIMATE_ATTR_SIZE;
    PyObject *res = NULL;
    static char *kwlist[] = {"item", "name", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oet|i" BYTES_CHAR, kwlist,
                                     &myarg, NULL, &attrname, &nofollow, &ns))
        return NULL;
    res = NULL;
    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        goto free_arg;
    }

    if(merge_ns(ns, attrname, &fullname, &namebuf) < 0) {
        goto free_tgt;
    }

    nret = _generic_get(_get_obj, &tgt, fullname, &buf, &nalloc, NULL);
    if(nret == -1) {
      goto free_buf;
    }

    /* Create the string which will hold the result */
    res = PyBytes_FromStringAndSize(buf, nret);

    /* Free the buffers, they are no longer needed */
 free_buf:
    PyMem_Free(buf);
    PyMem_Free(namebuf);
 free_tgt:
    free_tgt(&tgt);
 free_arg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

/* Wrapper for getxattr */
static char __get_all_doc__[] =
    "get_all(item[, nofollow=False, namespace=None])\n"
    "Get all the extended attributes of an item.\n"
    "\n"
    "This function performs a bulk-get of all extended attribute names\n"
    "and the corresponding value.\n"
    "Example:\n"
    "\n"
    "    >>> xattr.get_all('/path/to/file')\n"
    "    [('user.mime-type', 'plain/text'), ('user.comment', 'test'),\n"
    "     ('system.posix_acl_access', '\\x02\\x00...')]\n"
    "    >>> xattr.get_all('/path/to/file', namespace=xattr.NS_USER)\n"
    "    [('mime-type', 'plain/text'), ('comment', 'test')]\n"
    "\n"
    ITEM_DOC
    ":keyword namespace: an optional namespace for filtering the\n"
    "   attributes; for example, querying all user attributes can be\n"
    "   accomplished by passing namespace=:const:`NS_USER`\n"
    ":type namespace: string\n"
    NOFOLLOW_DOC
    ":return: list of tuples (name, value); note that if a namespace\n"
    "   argument was passed, it (and the separator) will be stripped from\n"
    "   the names returned\n"
    ":rtype: list\n"
    ":raises EnvironmentError: caused by any system errors\n"
    "\n"
    ".. note:: Since reading the whole attribute list is not an atomic\n"
    "   operation, it might be possible that attributes are added\n"
    "   or removed between the initial query and the actual reading\n"
    "   of the attributes; the returned list will contain only the\n"
    "   attributes that were present at the initial listing of the\n"
    "   attribute names and that were still present when the read\n"
    "   attempt for the value is made.\n"
    ".. versionadded:: 0.4\n"
    NS_CHANGED_DOC
    ;

static PyObject *
get_all(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg, *res;
    int nofollow=0;
    const char *ns = NULL;
    char *buf_list = NULL, *buf_val = NULL;
    const char *s;
    size_t nalloc = ESTIMATE_ATTR_SIZE;
    ssize_t nlist, nval;
    PyObject *mylist;
    target_t tgt;
    static char *kwlist[] = {"item", "nofollow", "namespace", NULL};
    int io_errno;

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|i" BYTES_CHAR, kwlist,
                                     &myarg, &nofollow, &ns))
        return NULL;
    if(convert_obj(myarg, &tgt, nofollow) < 0)
        return NULL;

    res = NULL;
    /* Compute first the list of attributes */
    nlist = _generic_get(_list_obj, &tgt, NULL, &buf_list,
                         &nalloc, &io_errno);
    if (nlist == -1) {
      /* We can't handle any errors, and the Python error is already
         set, just bail out. */
      goto free_tgt;
    }

    /* Create the list which will hold the result. */
    mylist = PyList_New(0);
    if(mylist == NULL) {
      goto free_buf_list;
    }

    nalloc = ESTIMATE_ATTR_SIZE;
    /* Create and insert the attributes as strings in the list */
    for(s = buf_list; s - buf_list < nlist; s += strlen(s) + 1) {
        PyObject *my_tuple;
        const char *name;

        if((name = matches_ns(ns, s)) == NULL)
            continue;
        /* Now retrieve the attribute value */
        nval = _generic_get(_get_obj, &tgt, s, &buf_val, &nalloc, &io_errno);
        if (nval == -1) {
          if (
#ifdef ENODATA
              io_errno == ENODATA ||
#endif
              io_errno == ENOATTR) {
            PyErr_Clear();
            continue;
          } else {
            Py_DECREF(mylist);
            goto free_buf_val;
          }
        }
        my_tuple = Py_BuildValue(BYTES_TUPLE, name, buf_val, nval);
        if (my_tuple == NULL) {
          Py_DECREF(mylist);
          goto free_buf_val;
        }
        PyList_Append(mylist, my_tuple);
        Py_DECREF(my_tuple);
    }

    /* Successful exit */
    res = mylist;

 free_buf_val:
    PyMem_Free(buf_val);

 free_buf_list:
    PyMem_Free(buf_list);

 free_tgt:
    free_tgt(&tgt);

    /* Return the result */
    return res;
}


static char __pysetxattr_doc__[] =
    "setxattr(item, name, value[, flags=0, nofollow=False])\n"
    "Set the value of a given extended attribute (deprecated).\n"
    "\n"
    "Be careful in case you want to set attributes on symbolic\n"
    "links, you have to use all the 5 parameters; use 0 for the \n"
    "flags value if you want the default behaviour (create or "
    "replace)\n"
    "\n"
    ITEM_DOC
    NAME_SET_DOC
    VALUE_DOC
    FLAGS_DOC
    NOFOLLOW_DOC
    "\n"
    ".. deprecated:: 0.4\n"
    "   this function has been deprecated\n"
    "   by the :func:`set` function.\n"
    ;

/* Wrapper for setxattr */
static PyObject *
pysetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL;
    char *buf = NULL;
    Py_ssize_t bufsize_s;
    size_t bufsize;
    int nret;
    int flags = 0;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oetet#|ii", &myarg, NULL, &attrname,
                          NULL, &buf, &bufsize_s, &flags, &nofollow))
        return NULL;

    if (bufsize_s < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "negative value size?!");
        res = NULL;
        goto free_arg;
    }
    bufsize = (size_t) bufsize_s;

    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        res = NULL;
        goto free_arg;
    }

    /* Set the attribute's value */
    nret = _set_obj(&tgt, attrname, buf, bufsize, flags);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto free_arg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 free_arg:
    PyMem_Free(attrname);
    PyMem_Free(buf);

    /* Return the result */
    return res;
}

static char __set_doc__[] =
    "set(item, name, value[, flags=0, namespace=None])\n"
    "Set the value of a given extended attribute.\n"
    "\n"
    "Example:\n"
    "\n"
    "    >>> xattr.set('/path/to/file', 'user.comment', 'test')\n"
    "    >>> xattr.set('/path/to/file', 'comment', 'test',"
    " namespace=xattr.NS_USER)\n"
    "\n"
    ITEM_DOC
    NAME_SET_DOC
    VALUE_DOC
    FLAGS_DOC
    NOFOLLOW_DOC
    NS_DOC
    ":returns: None\n"
    ":raises EnvironmentError: caused by any system errors\n"
    "\n"
    ".. versionadded:: 0.4\n"
    NS_CHANGED_DOC
    ;

/* Wrapper for setxattr */
static PyObject *
xattr_set(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL;
    char *buf = NULL;
    Py_ssize_t bufsize_s;
    size_t bufsize;
    int nret;
    int flags = 0;
    target_t tgt;
    const char *ns = NULL;
    char *newname;
    const char *full_name;
    static char *kwlist[] = {"item", "name", "value", "flags",
                             "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oetet#|ii" BYTES_CHAR,
                                     kwlist, &myarg, NULL, &attrname, NULL,
                                     &buf, &bufsize_s, &flags, &nofollow, &ns))
        return NULL;

    if (bufsize_s < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "negative value size?!");
        res = NULL;
        goto free_arg;
    }
    bufsize = (size_t) bufsize_s;

    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        res = NULL;
        goto free_arg;
    }

    if(merge_ns(ns, attrname, &full_name, &newname) < 0) {
        res = NULL;
        goto free_arg;
    }

    /* Set the attribute's value */
    nret = _set_obj(&tgt, full_name, buf, bufsize, flags);

    PyMem_Free(newname);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto free_arg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 free_arg:
    PyMem_Free(attrname);
    PyMem_Free(buf);

    /* Return the result */
    return res;
}


static char __pyremovexattr_doc__[] =
    "removexattr(item, name[, nofollow])\n"
    "Remove an attribute from a file (deprecated).\n"
    "\n"
    ITEM_DOC
    NAME_REMOVE_DOC
    NOFOLLOW_DOC
    "\n"
    ".. deprecated:: 0.4\n"
    "   this function has been deprecated by the :func:`remove` function.\n"
    ;

/* Wrapper for removexattr */
static PyObject *
pyremovexattr(PyObject *self, PyObject *args)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL;
    int nret;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oet|i", &myarg, NULL, &attrname, &nofollow))
        return NULL;

    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        res = NULL;
        goto free_arg;
    }

    /* Remove the attribute */
    nret = _remove_obj(&tgt, attrname);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto free_arg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 free_arg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

static char __remove_doc__[] =
    "remove(item, name[, nofollow=False, namespace=None])\n"
    "Remove an attribute from a file.\n"
    "\n"
    "Example:\n"
    "\n"
    "    >>> xattr.remove('/path/to/file', 'user.comment')\n"
    "\n"
    ITEM_DOC
    NAME_REMOVE_DOC
    NOFOLLOW_DOC
    NS_DOC
    ":returns: None\n"
    ":raises EnvironmentError: caused by any system errors\n"
    "\n"
    ".. versionadded:: 0.4\n"
    NS_CHANGED_DOC
    ;

/* Wrapper for removexattr */
static PyObject *
xattr_remove(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL, *name_buf;
    const char *ns = NULL;
    const char *full_name;
    int nret;
    target_t tgt;
    static char *kwlist[] = {"item", "name", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oet|i" BYTES_CHAR, kwlist,
                                     &myarg, NULL, &attrname, &nofollow, &ns))
        return NULL;

    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        res = NULL;
        goto free_arg;
    }

    if(merge_ns(ns, attrname, &full_name, &name_buf) < 0) {
        res = NULL;
        goto free_arg;
    }

    /* Remove the attribute */
    nret = _remove_obj(&tgt, full_name);

    PyMem_Free(name_buf);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto free_arg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 free_arg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

static char __pylistxattr_doc__[] =
    "listxattr(item[, nofollow=False])\n"
    "Return the list of attribute names for a file (deprecated).\n"
    "\n"
    ITEM_DOC
    NOFOLLOW_DOC
    "\n"
    ".. deprecated:: 0.4\n"
    "   this function has been deprecated by the :func:`list` function.\n"
    ;

/* Wrapper for listxattr */
static PyObject *
pylistxattr(PyObject *self, PyObject *args)
{
    char *buf = NULL;
    int nofollow = 0;
    ssize_t nret;
    size_t nalloc = ESTIMATE_ATTR_SIZE;
    PyObject *myarg;
    PyObject *mylist;
    Py_ssize_t nattrs;
    char *s;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "O|i", &myarg, &nofollow))
        return NULL;
    if(convert_obj(myarg, &tgt, nofollow) < 0)
        return NULL;

    nret = _generic_get(_list_obj, &tgt, NULL, &buf, &nalloc, NULL);
    if (nret == -1) {
      mylist = NULL;
      goto free_buf;
    }

    /* Compute the number of attributes in the list */
    for(s = buf, nattrs = 0; (s - buf) < nret; s += strlen(s) + 1) {
        nattrs++;
    }

    /* Create the list which will hold the result */
    mylist = PyList_New(nattrs);
    if(mylist == NULL) {
      goto free_buf;
    }

    /* Create and insert the attributes as strings in the list */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        PyObject *item = PyBytes_FromString(s);
        if(item == NULL) {
            Py_DECREF(mylist);
            mylist = NULL;
            goto free_buf;
        }
        PyList_SET_ITEM(mylist, nattrs, item);
        nattrs++;
    }

 free_buf:
    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);
    free_tgt(&tgt);

    /* Return the result */
    return mylist;
}

static char __list_doc__[] =
    "list(item[, nofollow=False, namespace=None])\n"
    "Return the list of attribute names for a file.\n"
    "\n"
    "Example:\n"
    "\n"
    "    >>> xattr.list('/path/to/file')\n"
    "    ['user.test', 'user.comment', 'system.posix_acl_access']\n"
    "    >>> xattr.list('/path/to/file', namespace=xattr.NS_USER)\n"
    "    ['test', 'comment']\n"
    "\n"
    ITEM_DOC
    NOFOLLOW_DOC
    NS_DOC
    ":returns: the list of attributes; note that if a namespace \n"
    "    argument was passed, it (and the separator) will be stripped\n"
    "    from the names\n"
    "    returned\n"
    ":rtype: list\n"
    ":raises EnvironmentError: caused by any system errors\n"
    "\n"
    ".. versionadded:: 0.4\n"
    NS_CHANGED_DOC
    ;

/* Wrapper for listxattr */
static PyObject *
xattr_list(PyObject *self, PyObject *args, PyObject *keywds)
{
    char *buf = NULL;
    int nofollow = 0;
    ssize_t nret;
    size_t nalloc = ESTIMATE_ATTR_SIZE;
    PyObject *myarg;
    PyObject *res;
    const char *ns = NULL;
    Py_ssize_t nattrs;
    char *s;
    target_t tgt;
    static char *kwlist[] = {"item", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|i" BYTES_CHAR, kwlist,
                                     &myarg, &nofollow, &ns))
        return NULL;
    res = NULL;
    if(convert_obj(myarg, &tgt, nofollow) < 0) {
        goto free_arg;
    }
    nret = _generic_get(_list_obj, &tgt, NULL, &buf, &nalloc, NULL);
    if (nret == -1) {
      goto free_tgt;
    }

    /* Compute the number of attributes in the list */
    for(s = buf, nattrs = 0; (s - buf) < nret; s += strlen(s) + 1) {
        if(matches_ns(ns, s) != NULL)
            nattrs++;
    }

    /* Create the list which will hold the result */
    if((res = PyList_New(nattrs)) == NULL) {
        goto free_buf;
    }

    /* Create and insert the attributes as strings in the list */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        const char *name = matches_ns(ns, s);
        if(name != NULL) {
            PyObject *item = PyBytes_FromString(name);
            if(item == NULL) {
                Py_DECREF(res);
                res = NULL;
                goto free_buf;
            }
            PyList_SET_ITEM(res, nattrs, item);
            nattrs++;
        }
    }

 free_buf:
    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

 free_tgt:
    free_tgt(&tgt);
 free_arg:

    /* Return the result */
    return res;
}

static PyMethodDef xattr_methods[] = {
    {"getxattr",  pygetxattr, METH_VARARGS, __pygetxattr_doc__ },
    {"get",  (PyCFunction) xattr_get, METH_VARARGS | METH_KEYWORDS,
     __get_doc__ },
    {"get_all", (PyCFunction) get_all, METH_VARARGS | METH_KEYWORDS,
     __get_all_doc__ },
    {"setxattr",  pysetxattr, METH_VARARGS, __pysetxattr_doc__ },
    {"set",  (PyCFunction) xattr_set, METH_VARARGS | METH_KEYWORDS,
     __set_doc__ },
    {"removexattr",  pyremovexattr, METH_VARARGS, __pyremovexattr_doc__ },
    {"remove",  (PyCFunction) xattr_remove, METH_VARARGS | METH_KEYWORDS,
     __remove_doc__ },
    {"listxattr",  pylistxattr, METH_VARARGS, __pylistxattr_doc__ },
    {"list",  (PyCFunction) xattr_list, METH_VARARGS | METH_KEYWORDS,
     __list_doc__ },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static char __xattr_doc__[] = \
    "This module gives access to the extended attributes present\n"
    "in some operating systems/filesystems. You can list attributes,\n"
    "get, set and remove them.\n"
    "\n"
    "The module exposes two sets of functions:\n"
    "  - the 'old' :func:`listxattr`, :func:`getxattr`, :func:`setxattr`,\n"
    "    :func:`removexattr`\n"
    "    functions which are deprecated since version 0.4\n"
    "  - the new :func:`list`, :func:`get`, :func:`get_all`, :func:`set`,\n"
    "    :func:`remove` functions\n"
    "    which expose a namespace-aware API and simplify a bit the calling\n"
    "    model by using keyword arguments\n"
    "\n"
    "Example: \n\n"
    "  >>> import xattr\n"
    "  >>> xattr.listxattr(\"file.txt\")\n"
    "  ['user.mime_type']\n"
    "  >>> xattr.getxattr(\"file.txt\", \"user.mime_type\")\n"
    "  'text/plain'\n"
    "  >>> xattr.setxattr(\"file.txt\", \"user.comment\", "
    "\"Simple text file\")\n"
    "  >>> xattr.listxattr(\"file.txt\")\n"
    "  ['user.mime_type', 'user.comment']\n"
    "  >>> xattr.removexattr (\"file.txt\", \"user.comment\")\n"
    "\n"
    ".. note:: Most or all errors reported by the system while using\n"
    "   the ``xattr`` library will be reported by raising\n"
    "   a :exc:`EnvironmentError`; under\n"
    "   Linux, the following ``errno`` values are used:\n"
    "\n"
    "   - ``ENOATTR`` and ``ENODATA`` mean that the attribute name is\n"
    "     invalid\n"
    "   - ``ENOTSUP`` and ``EOPNOTSUPP`` mean that the filesystem does not\n"
    "     support extended attributes, or that the namespace is invalid\n"
    "   - ``E2BIG`` mean that the attribute value is too big\n"
    "   - ``ERANGE`` mean that the attribute name is too big (it might also\n"
    "     mean an error in the xattr module itself)\n"
    "   - ``ENOSPC`` and ``EDQUOT`` are documented as meaning out of disk\n"
    "     space or out of disk space because of quota limits\n"
    ".. note:: Under Python 3, the namespace argument is a byte string,\n"
    "   not a unicode string.\n"
    "\n"
    ;

#ifdef IS_PY3K

static struct PyModuleDef xattrmodule = {
    PyModuleDef_HEAD_INIT,
    "xattr",
    __xattr_doc__,
    0,
    xattr_methods,
};

#define INITERROR return NULL

PyMODINIT_FUNC
PyInit_xattr(void)

#else
#define INITERROR return
void
initxattr(void)
#endif
{
    PyObject *ns_security = NULL;
    PyObject *ns_system   = NULL;
    PyObject *ns_trusted  = NULL;
    PyObject *ns_user     = NULL;
#ifdef IS_PY3K
    PyObject *m = PyModule_Create(&xattrmodule);
#else
    PyObject *m = Py_InitModule3("xattr", xattr_methods, __xattr_doc__);
#endif
    if (m==NULL)
        INITERROR;

    PyModule_AddStringConstant(m, "__author__", _XATTR_AUTHOR);
    PyModule_AddStringConstant(m, "__contact__", _XATTR_EMAIL);
    PyModule_AddStringConstant(m, "__version__", _XATTR_VERSION);
    PyModule_AddStringConstant(m, "__license__",
                               "GNU Lesser General Public License (LGPL)");
    PyModule_AddStringConstant(m, "__docformat__", "restructuredtext en");

    PyModule_AddIntConstant(m, "XATTR_CREATE", XATTR_CREATE);
    PyModule_AddIntConstant(m, "XATTR_REPLACE", XATTR_REPLACE);

    /* namespace constants */
    if((ns_security = PyBytes_FromString("security")) == NULL)
        goto err_out;
    if((ns_system = PyBytes_FromString("system")) == NULL)
        goto err_out;
    if((ns_trusted = PyBytes_FromString("trusted")) == NULL)
        goto err_out;
    if((ns_user = PyBytes_FromString("user")) == NULL)
        goto err_out;
    if(PyModule_AddObject(m, "NS_SECURITY", ns_security) < 0)
        goto err_out;
    ns_security = NULL;
    if(PyModule_AddObject(m, "NS_SYSTEM", ns_system) < 0)
        goto err_out;
    ns_system = NULL;
    if(PyModule_AddObject(m, "NS_TRUSTED", ns_trusted) < 0)
        goto err_out;
    ns_trusted = NULL;
    if(PyModule_AddObject(m, "NS_USER", ns_user) < 0)
        goto err_out;
    ns_user = NULL;

#ifdef IS_PY3K
    return m;
#else
    return;
#endif

 err_out:
    Py_XDECREF(ns_user);
    Py_XDECREF(ns_trusted);
    Py_XDECREF(ns_system);
    Py_XDECREF(ns_security);
    INITERROR;
}
