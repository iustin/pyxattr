/*
    xattr - a python module for manipulating filesystem extended attributes

    Copyright (C) 2002, 2003, 2006, 2008 Iustin Pop <iusty@k1024.org>

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
#include <attr/xattr.h>
#include <stdio.h>

/* Compatibility with python 2.4 regarding python size type (PEP 353) */
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#else
#define PyBytes_Check PyString_Check
#define PyBytes_AS_STRING PyString_AS_STRING
#define PyBytes_FromStringAndSize PyString_FromStringAndSize
#define PyBytes_FromString PyString_FromString
#endif

/* the estimated (startup) attribute buffer size in
   multi-operations */
#define ESTIMATE_ATTR_SIZE 256

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

/** Converts from a string, file or int argument to what we need. */
static int convertObj(PyObject *myobj, target_t *tgt, int nofollow) {
    int fd;
    tgt->tmp = NULL;
    if(PyBytes_Check(myobj)) {
        tgt->type = nofollow ? T_LINK : T_PATH;
        tgt->name = PyBytes_AS_STRING(myobj);
    } else if(PyUnicode_Check(myobj)) {
        tgt->type = nofollow ? T_LINK : T_PATH;
        tgt->tmp = \
            PyUnicode_AsEncodedString(myobj,
                                      Py_FileSystemDefaultEncoding, "strict");
        if(tgt->tmp == NULL)
            return 0;
        tgt->name = PyBytes_AS_STRING(tgt->tmp);
    } else if((fd = PyObject_AsFileDescriptor(myobj)) != -1) {
        tgt->type = T_FD;
        tgt->fd = fd;
    } else {
        PyErr_SetString(PyExc_TypeError, "argument must be string or int");
        return 0;
    }
    return 1;
}

/* Combine a namespace string and an attribute name into a
   fully-qualified name */
static const char* merge_ns(const char *ns, const char *name, char **buf) {
    if(ns != NULL) {
        int cnt;
        size_t new_size = strlen(ns) + 1 + strlen(name) + 1;
        if((*buf = PyMem_Malloc(new_size)) == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
        cnt = snprintf(*buf, new_size, "%s.%s", ns, name);
        if(cnt > new_size || cnt < 0) {
            PyErr_SetString(PyExc_ValueError,
                            "can't format the attribute name");
            PyMem_Free(*buf);
            return NULL;
        }
        return *buf;
    } else {
        *buf = NULL;
        return name;
    }
}

static ssize_t _list_obj(target_t *tgt, char *list, size_t size) {
    if(tgt->type == T_FD)
        return flistxattr(tgt->fd, list, size);
    else if (tgt->type == T_LINK)
        return llistxattr(tgt->name, list, size);
    else
        return listxattr(tgt->name, list, size);
}

static ssize_t _get_obj(target_t *tgt, const char *name, void *value,
                        size_t size) {
    if(tgt->type == T_FD)
        return fgetxattr(tgt->fd, name, value, size);
    else if (tgt->type == T_LINK)
        return lgetxattr(tgt->name, name, value, size);
    else
        return getxattr(tgt->name, name, value, size);
}

static int _set_obj(target_t *tgt, const char *name,
                    const void *value, size_t size, int flags) {
    if(tgt->type == T_FD)
        return fsetxattr(tgt->fd, name, value, size, flags);
    else if (tgt->type == T_LINK)
        return lsetxattr(tgt->name, name, value, size, flags);
    else
        return setxattr(tgt->name, name, value, size, flags);
}

static int _remove_obj(target_t *tgt, const char *name) {
    if(tgt->type == T_FD)
        return fremovexattr(tgt->fd, name);
    else if (tgt->type == T_LINK)
        return lremovexattr(tgt->name, name);
    else
        return removexattr(tgt->name, name);
}

/*
   Checks if an attribute name matches an optional namespace.

   If the namespace is NULL, it will return the name itself.  If the
   namespace is non-NULL and the name matches, it will return a
   pointer to the offset in the name after the namespace and the
   separator. If however the name doesn't match the namespace, it will
   return NULL.
*/
const char *matches_ns(const char *ns, const char *name) {
    size_t ns_size;
    if (ns == NULL)
        return name;
    ns_size = strlen(ns);

    if (strlen(name) > (ns_size+1) && !strncmp(name, ns, ns_size) &&
        name[ns_size] == '.')
        return name + ns_size + 1;
    return NULL;
}

/* Wrapper for getxattr */
static char __pygetxattr_doc__[] =
    "Get the value of a given extended attribute (deprecated).\n"
    "\n"
    "Parameters:\n"
    "  - a string representing filename, or a file-like object,\n"
    "    or a file descriptor; this represents the file on \n"
    "    which to act\n"
    "  - a string, representing the attribute whose value to retrieve;\n"
    "    usually in form of system.posix_acl or user.mime_type\n"
    "  - (optional) a boolean value (defaults to false), which, if\n"
    "    the file name given is a symbolic link, makes the\n"
    "    function operate on the symbolic link itself instead\n"
    "    of its target;\n"
    "@deprecated: since version 0.4, this function has been deprecated\n"
    "    by the L{get} function\n"
    ;

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    target_t tgt;
    int nofollow = 0;
    char *attrname = NULL;
    char *buf;
    ssize_t nalloc, nret;
    PyObject *res;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oet|i", &myarg, NULL, &attrname, &nofollow))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    /* Find out the needed size of the buffer */
    if((nalloc = _get_obj(&tgt, attrname, NULL, 0)) == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freetgt;
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        res = PyErr_NoMemory();
        goto freetgt;
    }

    /* Now retrieve the attribute value */
    if((nret = _get_obj(&tgt, attrname, buf, nalloc)) == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freebuf;
    }

    /* Create the string which will hold the result */
    res = PyBytes_FromStringAndSize(buf, nret);

 freebuf:
    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);
 freetgt:
    free_tgt(&tgt);
 freearg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

/* Wrapper for getxattr */
static char __get_doc__[] =
    "Get the value of a given extended attribute.\n"
    "\n"
    "Example:\n"
    "    >>> xattr.get('/path/to/file', 'user.comment')\n"
    "    'test'\n"
    "    >>> xattr.get('/path/to/file', 'comment', namespace=xattr.NS_USER)\n"
    "    'test'\n"
    "\n"
    "@param item: the item to query; either a string representing the\n"
    "    filename, or a file-like object, or a file descriptor\n"
    "@param name: the attribute whose value to set; usually in form of\n"
    "    system.posix_acl or user.mime_type\n"
    "@type name: string\n"
    "@param nofollow: if given and True, and the function is passed a\n"
    "    filename that points to a symlink, the function will act on the\n"
    "    symlink itself instead of its target\n"
    "@type nofollow: boolean\n"
    "@param namespace: if given, the attribute must not contain the\n"
    "    namespace itself, but instead the namespace will be taken from\n"
    "    this parameter\n"
    "@type namespace: string\n"
    "@return: the value of the extended attribute (can contain NULLs)\n"
    "@rtype: string\n"
    "@raise EnvironmentError: caused by any system errors\n"
    "@since: 0.4\n"
    ;

static PyObject *
xattr_get(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg;
    target_t tgt;
    int nofollow = 0;
    char *attrname = NULL, *namebuf;
    const char *fullname;
    char *buf;
    char *ns = NULL;
    ssize_t nalloc, nret;
    PyObject *res;
    static char *kwlist[] = {"item", "name", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oet|iz", kwlist,
                                     &myarg, NULL, &attrname, &nofollow, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    fullname = merge_ns(ns, attrname, &namebuf);

    /* Find out the needed size of the buffer */
    if((nalloc = _get_obj(&tgt, fullname, NULL, 0)) == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freetgt;
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        res = PyErr_NoMemory();
        goto freenamebuf;
    }

    /* Now retrieve the attribute value */
    if((nret = _get_obj(&tgt, fullname, buf, nalloc)) == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freebuf;
    }

    /* Create the string which will hold the result */
    res = PyBytes_FromStringAndSize(buf, nret);

    /* Free the buffers, they are no longer needed */
 freebuf:
    PyMem_Free(buf);
 freenamebuf:
    PyMem_Free(namebuf);
 freetgt:
    free_tgt(&tgt);
 freearg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

/* Wrapper for getxattr */
static char __get_all_doc__[] =
    "Get all the extended attributes of an item.\n"
    "\n"
    "This function performs a bulk-get of all extended attribute names\n"
    "and the corresponding value.\n"
    "Example:\n"
    "    >>> xattr.get_all('/path/to/file')\n"
    "    [('user.mime-type', 'plain/text'), ('user.comment', 'test'),\n"
    "     ('system.posix_acl_access', '\\x02\\x00...')]\n"
    "    >>> xattr.get_all('/path/to/file', namespace=xattr.NS_USER)\n"
    "    [('mime-type', 'plain/text'), ('comment', 'test')]\n"
    "\n"
    "@param item: the item to query; either a string representing the\n"
    "    filename, or a file-like object, or a file descriptor\n"
    "@keyword namespace: an optional namespace for filtering the\n"
    "    attributes; for example, querying all user attributes can be\n"
    "    accomplished by passing namespace=L{NS_USER}\n"
    "@type namespace: string\n"
    "@keyword nofollow: if passed and true, if the target file is a\n"
    "    symbolic link, the attributes for the link itself will be\n"
    "    returned, instead of the attributes of the target\n"
    "@type nofollow: boolean\n"
    "@return: list of tuples (name, value); note that if a namespace\n"
    "    argument was passed, it (and the separator) will be stripped from\n"
    "    the names returned\n"
    "@rtype: list\n"
    "@raise EnvironmentError: caused by any system errors\n"
    "@note: Since reading the whole attribute list is not an atomic\n"
    "    operation, it might be possible that attributes are added\n"
    "    or removed between the initial query and the actual reading\n"
    "    of the attributes; the returned list will contain only the\n"
    "    attributes that were present at the initial listing of the\n"
    "    attribute names and that were still present when the read\n"
    "    attempt for the value is made.\n"
    "@since: 0.4\n"
    ;

static PyObject *
get_all(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg, *res;
    int dolink=0;
    char *ns = NULL;
    char *buf_list, *buf_val;
    char *s;
    size_t nalloc, nlist, nval;
    PyObject *mylist;
    target_t tgt;
    static char *kwlist[] = {"item", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|iz", kwlist,
                                     &myarg, &dolink, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, dolink))
        return NULL;

    /* Compute first the list of attributes */

    /* Find out the needed size of the buffer for the attribute list */
    nalloc = _list_obj(&tgt, NULL, 0);

    if(nalloc == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freetgt;
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf_list = PyMem_Malloc(nalloc)) == NULL) {
        res = PyErr_NoMemory();
        goto freetgt;
    }

    /* Now retrieve the list of attributes */
    nlist = _list_obj(&tgt, buf_list, nalloc);

    if(nlist == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto free_buf_list;
    }

    /* Create the list which will hold the result */
    mylist = PyList_New(0);
    nalloc = ESTIMATE_ATTR_SIZE;
    if((buf_val = PyMem_Malloc(nalloc)) == NULL) {
        Py_DECREF(mylist);
        res = PyErr_NoMemory();
        goto free_buf_list;
    }

    /* Create and insert the attributes as strings in the list */
    for(s = buf_list; s - buf_list < nlist; s += strlen(s) + 1) {
        PyObject *my_tuple;
        int missing;
        const char *name;

        if((name=matches_ns(ns, s))==NULL)
            continue;
        /* Now retrieve the attribute value */
        missing = 0;
        while(1) {
            nval = _get_obj(&tgt, s, buf_val, nalloc);

            if(nval == -1) {
                if(errno == ERANGE) {
                    nval = _get_obj(&tgt, s, NULL, 0);
                    if((buf_val = PyMem_Realloc(buf_val, nval)) == NULL) {
                        res = NULL;
                        Py_DECREF(mylist);
                        goto free_buf_list;
                    }
                    nalloc = nval;
                    continue;
                } else if(errno == ENODATA || errno == ENOATTR) {
                    /* this attribute has gone away since we queried
                       the attribute list */
                    missing = 1;
                    break;
                }
                res = PyErr_SetFromErrno(PyExc_IOError);
                goto freebufval;
            }
            break;
        }
        if(missing)
            continue;
#ifdef IS_PY3K
        my_tuple = Py_BuildValue("yy#", name, buf_val, nval);
#else
        my_tuple = Py_BuildValue("ss#", name, buf_val, nval);
#endif

        PyList_Append(mylist, my_tuple);
        Py_DECREF(my_tuple);
    }

    /* Successfull exit */
    res = mylist;

 freebufval:
    PyMem_Free(buf_val);

 free_buf_list:
    PyMem_Free(buf_list);

 freetgt:
    free_tgt(&tgt);

    /* Return the result */
    return res;
}


static char __pysetxattr_doc__[] =
    "Set the value of a given extended attribute (deprecated).\n"
    "\n"
    "Be carefull in case you want to set attributes on symbolic\n"
    "links, you have to use all the 5 parameters; use 0 for the \n"
    "flags value if you want the default behavior (create or "
    "replace)\n"
    "\n"
    "Parameters:\n"
    "  - a string representing filename, or a file-like object,\n"
    "    or a file descriptor; this represents the file on \n"
    "    which to act\n"
    "  - a string, representing the attribute whose value to set;\n"
    "    usually in form of system.posix_acl or user.mime_type\n"
    "  - a string, possibly with embedded NULLs; note that there\n"
    "    are restrictions regarding the size of the value, for\n"
    "    example, for ext2/ext3, maximum size is the block size\n"
    "  - (optional) flags; if 0 or ommited the attribute will be \n"
    "    created or replaced; if XATTR_CREATE, the attribute \n"
    "    will be created, giving an error if it already exists;\n"
    "    of XATTR_REPLACE, the attribute will be replaced,\n"
    "    giving an error if it doesn't exists;\n"
    "  - (optional) a boolean value (defaults to false), which, if\n"
    "    the file name given is a symbolic link, makes the\n"
    "    function operate on the symbolic link itself instead\n"
    "    of its target;\n"
    "@deprecated: since version 0.4, this function has been deprecated\n"
    "    by the L{set} function\n"
    ;

/* Wrapper for setxattr */
static PyObject *
pysetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL;
    char *buf = NULL;
    Py_ssize_t bufsize;
    int nret;
    int flags = 0;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oetet#|bi", &myarg, NULL, &attrname,
                          NULL, &buf, &bufsize, &flags, &nofollow))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    /* Set the attribute's value */
    nret = _set_obj(&tgt, attrname, buf, bufsize, flags);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freearg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 freearg:
    PyMem_Free(attrname);
    PyMem_Free(buf);

    /* Return the result */
    return res;
}

static char __set_doc__[] =
    "Set the value of a given extended attribute.\n"
    "\n"
    "Example:\n"
    "    >>> xattr.set('/path/to/file', 'user.comment', 'test')\n"
    "    >>> xattr.set('/path/to/file', 'comment', 'test',"
    " namespace=xattr.NS_USER)\n"
    "\n"
    "@param item: the item to query; either a string representing the\n"
    "    filename, or a file-like object, or a file descriptor\n"
    "@param name: the attribute whose value to set; usually in form of\n"
    "    system.posix_acl or user.mime_type\n"
    "@type name: string\n"
    "@param value: a string, possibly with embedded NULLs; note that there\n"
    "    are restrictions regarding the size of the value, for\n"
    "    example, for ext2/ext3, maximum size is the block size\n"
    "@type value: string\n"
    "@param flags: if 0 or ommited the attribute will be\n"
    "    created or replaced; if L{XATTR_CREATE}, the attribute\n"
    "    will be created, giving an error if it already exists;\n"
    "    if L{XATTR_REPLACE}, the attribute will be replaced,\n"
    "    giving an error if it doesn't exists;\n"
    "@type flags: integer\n"
    "@param nofollow: if given and True, and the function is passed a\n"
    "    filename that points to a symlink, the function will act on the\n"
    "    symlink itself instead of its target\n"
    "@type nofollow: boolean\n"
    "@param namespace: if given, the attribute must not contain the\n"
    "    namespace itself, but instead the namespace will be taken from\n"
    "    this parameter\n"
    "@type namespace: string\n"
    "@rtype: None\n"
    "@raise EnvironmentError: caused by any system errors\n"
    "@since: 0.4\n"
    ;

/* Wrapper for setxattr */
static PyObject *
xattr_set(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL;
    char *buf = NULL;
    Py_ssize_t bufsize;
    int nret;
    int flags = 0;
    target_t tgt;
    char *ns = NULL;
    char *newname;
    const char *full_name;
    static char *kwlist[] = {"item", "name", "value", "flags",
                             "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oetet#|iiz", kwlist,
                                     &myarg, NULL, &attrname, NULL,
                                     &buf, &bufsize, &flags, &nofollow, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    full_name = merge_ns(ns, attrname, &newname);

    /* Set the attribute's value */
    nret = _set_obj(&tgt, full_name, buf, bufsize, flags);

    if(newname != NULL)
        PyMem_Free(newname);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freearg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 freearg:
    PyMem_Free(attrname);
    PyMem_Free(buf);

    /* Return the result */
    return res;
}


static char __pyremovexattr_doc__[] =
    "Remove an attribute from a file (deprecated).\n"
    "\n"
    "Parameters:\n"
    "  - a string representing filename, or a file-like object,\n"
    "    or a file descriptor; this represents the file on \n"
    "    which to act\n"
    "  - a string, representing the attribute to be removed;\n"
    "    usually in form of system.posix_acl or user.mime_type\n"
    "  - (optional) a boolean value (defaults to false), which, if\n"
    "    the file name given is a symbolic link, makes the\n"
    "    function operate on the symbolic link itself instead\n"
    "    of its target;\n"
    "@deprecated: since version 0.4, this function has been deprecated\n"
    "    by the L{remove}"
    " function\n"
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

    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    /* Remove the attribute */
    nret = _remove_obj(&tgt, attrname);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freearg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 freearg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

static char __remove_doc__[] =
    "Remove an attribute from a file.\n"
    "\n"
    "Example:\n"
    "    >>> xattr.remove('/path/to/file', 'user.comment')\n"
    "\n"
    "@param item: the item to query; either a string representing the\n"
    "    filename, or a file-like object, or a file descriptor\n"
    "@param name: the attribute whose value to set; usually in form of\n"
    "    system.posix_acl or user.mime_type\n"
    "@type name: string\n"
    "@param nofollow: if given and True, and the function is passed a\n"
    "    filename that points to a symlink, the function will act on the\n"
    "    symlink itself instead of its target\n"
    "@type nofollow: boolean\n"
    "@param namespace: if given, the attribute must not contain the\n"
    "    namespace itself, but instead the namespace will be taken from\n"
    "    this parameter\n"
    "@type namespace: string\n"
    "@since: 0.4\n"
    "@rtype: None\n"
    "@raise EnvironmentError: caused by any system errors\n"
    ;

/* Wrapper for removexattr */
static PyObject *
xattr_remove(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg, *res;
    int nofollow = 0;
    char *attrname = NULL, *name_buf;
    char *ns = NULL;
    const char *full_name;
    int nret;
    target_t tgt;
    static char *kwlist[] = {"item", "name", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oet|iz", kwlist,
                                     &myarg, NULL, &attrname, &nofollow, &ns))
        return NULL;

    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    full_name = merge_ns(ns, attrname, &name_buf);
    if(full_name == NULL) {
        res = NULL;
        goto freearg;
    }

    /* Remove the attribute */
    nret = _remove_obj(&tgt, full_name);

    PyMem_Free(name_buf);

    free_tgt(&tgt);

    if(nret == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freearg;
    }

    Py_INCREF(Py_None);
    res = Py_None;

 freearg:
    PyMem_Free(attrname);

    /* Return the result */
    return res;
}

static char __pylistxattr_doc__[] =
    "Return the list of attribute names for a file (deprecated).\n"
    "\n"
    "Parameters:\n"
    "  - a string representing filename, or a file-like object,\n"
    "    or a file descriptor; this represents the file to \n"
    "    be queried\n"
    "  - (optional) a boolean value (defaults to false), which, if\n"
    "    the file name given is a symbolic link, makes the\n"
    "    function operate on the symbolic link itself instead\n"
    "    of its target;\n"
    "@deprecated: since version 0.4, this function has been deprecated\n"
    "    by the L{list}"
    " function\n"
    ;

/* Wrapper for listxattr */
static PyObject *
pylistxattr(PyObject *self, PyObject *args)
{
    char *buf;
    int nofollow=0;
    ssize_t nalloc, nret;
    PyObject *myarg;
    PyObject *mylist;
    Py_ssize_t nattrs;
    char *s;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "O|i", &myarg, &nofollow))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    /* Find out the needed size of the buffer */
    if((nalloc = _list_obj(&tgt, NULL, 0)) == -1) {
        mylist = PyErr_SetFromErrno(PyExc_IOError);
        goto freetgt;
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        mylist = PyErr_NoMemory();
        goto freetgt;
    }

    /* Now retrieve the list of attributes */
    if((nret = _list_obj(&tgt, buf, nalloc)) == -1) {
        mylist = PyErr_SetFromErrno(PyExc_IOError);
        goto freebuf;
    }

    /* Compute the number of attributes in the list */
    for(s = buf, nattrs = 0; (s - buf) < nret; s += strlen(s) + 1) {
        nattrs++;
    }

    /* Create the list which will hold the result */
    mylist = PyList_New(nattrs);

    /* Create and insert the attributes as strings in the list */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        PyList_SET_ITEM(mylist, nattrs, PyBytes_FromString(s));
        nattrs++;
    }

 freebuf:
    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

 freetgt:
    free_tgt(&tgt);

    /* Return the result */
    return mylist;
}

static char __list_doc__[] =
    "Return the list of attribute names for a file.\n"
    "\n"
    "Example:\n"
    "    >>> xattr.list('/path/to/file')\n"
    "    ['user.test', 'user.comment', 'system.posix_acl_access']\n"
    "    >>> xattr.list('/path/to/file', namespace=xattr.NS_USER)\n"
    "    ['test', 'comment']\n"
    "\n"
    "@param item: the item to query; either a string representing the\n"
    "    filename, or a file-like object, or a file descriptor\n"
    "@param nofollow: if given and True, and the function is passed a\n"
    "    filename that points to a symlink, the function will act on the\n"
    "    symlink itself instead of its target\n"
    "@type nofollow: boolean\n"
    "@param namespace: if given, the attribute must not contain the\n"
    "    namespace itself, but instead the namespace will be taken from\n"
    "    this parameter\n"
    "@type namespace: string\n"
    "@return: list of strings; note that if a namespace argument was\n"
    "    passed, it (and the separator) will be stripped from the names\n"
    "    returned\n"
    "@rtype: list\n"
    "@raise EnvironmentError: caused by any system errors\n"
    "@since: 0.4\n"
    ;

/* Wrapper for listxattr */
static PyObject *
xattr_list(PyObject *self, PyObject *args, PyObject *keywds)
{
    char *buf;
    int nofollow = 0;
    ssize_t nalloc, nret;
    PyObject *myarg;
    PyObject *res;
    char *ns = NULL;
    Py_ssize_t nattrs;
    char *s;
    target_t tgt;
    static char *kwlist[] = {"item", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|iet", kwlist,
                                     &myarg, &nofollow, NULL, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow)) {
        res = NULL;
        goto freearg;
    }

    /* Find out the needed size of the buffer */
    if((nalloc = _list_obj(&tgt, NULL, 0)) == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freetgt;
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        res = PyErr_NoMemory();
        goto freetgt;
    }

    /* Now retrieve the list of attributes */
    if((nret = _list_obj(&tgt, buf, nalloc)) == -1) {
        res = PyErr_SetFromErrno(PyExc_IOError);
        goto freebuf;
    }

    /* Compute the number of attributes in the list */
    for(s = buf, nattrs = 0; (s - buf) < nret; s += strlen(s) + 1) {
        if(matches_ns(ns, s) != NULL)
            nattrs++;
    }
    /* Create the list which will hold the result */
    res = PyList_New(nattrs);

    /* Create and insert the attributes as strings in the list */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        const char *name = matches_ns(ns, s);
        if(name!=NULL) {
            PyList_SET_ITEM(res, nattrs, PyBytes_FromString(name));
            nattrs++;
        }
    }

 freebuf:
    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

 freetgt:
    free_tgt(&tgt);
 freearg:
    PyMem_Free(ns);

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
    "Interface to extended filesystem attributes.\n"
    "\n"
    "This module gives access to the extended attributes present\n"
    "in some operating systems/filesystems. You can list attributes,\n"
    "get, set and remove them.\n"
    "\n"
    "The module exposes two sets of functions:\n"
    "  - the 'old' L{listxattr}, L{getxattr}, L{setxattr}, L{removexattr}\n"
    "    functions which are deprecated since version 0.4\n"
    "  - the new L{list}, L{get}, L{get_all}, L{set}, L{remove} functions\n"
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
    "@note: Most or all errors reported by the system while using the xattr\n"
    "library will be reported by raising a L{EnvironmentError}; under Linux,\n"
    "the following C{errno} values are used:\n"
    "  - C{ENOATTR} and C{ENODATA} mean that the attribute name is invalid\n"
    "  - C{ENOTSUP} and C{EOPNOTSUPP} mean that the filesystem does not\n"
    "    support extended attributes, or that the namespace is invalid\n"
    "  - C{E2BIG} mean that the attribute value is too big\n"
    "  - C{ERANGE} mean that the attribute name is too big (it might also\n"
    "    mean an error in the xattr module itself)\n"
    "  - C{ENOSPC} and C{EDQUOT} are documented as meaning out of disk space\n"
    "    or out of disk space because of quota limits\n"
    "\n"
    "@group Deprecated API: *xattr\n"
    "@group Namespace constants: NS_*\n"
    "@group set function flags: XATTR_CREATE, XATTR_REPLACE\n"
    "@sort: list, get, get_all, set, remove, listxattr, getxattr, setxattr\n"
    "    removexattr\n"
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
    PyModule_AddStringConstant(m, "__docformat__", "epytext en");

    PyModule_AddIntConstant(m, "XATTR_CREATE", XATTR_CREATE);
    PyModule_AddIntConstant(m, "XATTR_REPLACE", XATTR_REPLACE);

    /* namespace constants */
    PyModule_AddObject(m, "NS_SECURITY", PyBytes_FromString("security"));
    PyModule_AddObject(m, "NS_SYSTEM", PyBytes_FromString("system"));
    PyModule_AddObject(m, "NS_TRUSTED", PyBytes_FromString("trusted"));
    PyModule_AddObject(m, "NS_USER", PyBytes_FromString("user"));

#ifdef IS_PY3K
    return m;
#endif
}
