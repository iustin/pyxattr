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
} target_t;

/** Converts from a string, file or int argument to what we need. */
static int convertObj(PyObject *myobj, target_t *tgt, int nofollow) {
    int fd;
    if(PyString_Check(myobj)) {
        tgt->type = nofollow ? T_LINK : T_PATH;
        tgt->name = PyString_AS_STRING(myobj);
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
    int nofollow=0;
    char *attrname;
    char *buf;
    ssize_t nalloc, nret;
    PyObject *res;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os|i", &myarg, &attrname, &nofollow))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    /* Find out the needed size of the buffer */
    if((nalloc = _get_obj(&tgt, attrname, NULL, 0)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the attribute value */
    if((nret = _get_obj(&tgt, attrname, buf, nalloc)) == -1) {
        PyMem_Free(buf);
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Create the string which will hold the result */
    res = PyString_FromStringAndSize(buf, nret);

    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

    /* Return the result */
    return res;
}

/* Wrapper for getxattr */
static char __get_doc__[] =
    "Get the value of a given extended attribute.\n"
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
    int nofollow=0;
    char *attrname, *namebuf;
    const char *fullname;
    char *buf;
    char *ns = NULL;
    ssize_t nalloc, nret;
    PyObject *res;
    static char *kwlist[] = {"item", "name", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Os|iz", kwlist,
                                     &myarg, &attrname, &nofollow, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    fullname = merge_ns(ns, attrname, &namebuf);

    /* Find out the needed size of the buffer */
    if((nalloc = _get_obj(&tgt, fullname, NULL, 0)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyMem_Free(namebuf);
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the attribute value */
    if((nret = _get_obj(&tgt, fullname, buf, nalloc)) == -1) {
        PyMem_Free(buf);
        PyMem_Free(namebuf);
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Create the string which will hold the result */
    res = PyString_FromStringAndSize(buf, nret);

    /* Free the buffers, they are no longer needed */
    PyMem_Free(namebuf);
    PyMem_Free(buf);

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
    PyObject *myarg;
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
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf_list = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the list of attributes */
    nlist = _list_obj(&tgt, buf_list, nalloc);

    if(nlist == -1) {
        PyErr_SetFromErrno(PyExc_IOError);
        goto free_buf_list;
    }

    /* Create the list which will hold the result */
    mylist = PyList_New(0);
    nalloc = ESTIMATE_ATTR_SIZE;
    if((buf_val = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        goto free_list;
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
                    if((buf_val = PyMem_Realloc(buf_val, nval)) == NULL)
                        goto free_list;
                    nalloc = nval;
                    continue;
                } else if(errno == ENODATA || errno == ENOATTR) {
                    /* this attribute has gone away since we queried
                       the attribute list */
                    missing = 1;
                    break;
                }
                goto exit_errno;
            }
            break;
        }
        if(missing)
            continue;
        my_tuple = Py_BuildValue("ss#", name, buf_val, nval);

        PyList_Append(mylist, my_tuple);
        Py_DECREF(my_tuple);
    }

    /* Free the buffers, now they are no longer needed */
    PyMem_Free(buf_val);
    PyMem_Free(buf_list);

    /* Return the result */
    return mylist;
 exit_errno:
    PyErr_SetFromErrno(PyExc_IOError);
    PyMem_Free(buf_val);
 free_list:
    Py_DECREF(mylist);
 free_buf_list:
    PyMem_Free(buf_list);
    return NULL;
}


static char __pysetxattr_doc__[] =
    "Set the value of a given extended attribute (deprecated).\n"
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
    PyObject *myarg;
    int nofollow=0;
    char *attrname;
    char *buf;
    Py_ssize_t bufsize;
    int nret;
    int flags = 0;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oss#|bi", &myarg, &attrname,
                          &buf, &bufsize, &flags, &nofollow))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    /* Set the attribute's value */
    if((nret = _set_obj(&tgt, attrname, buf, bufsize, flags)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Return the result */
    Py_RETURN_NONE;
}

static char __set_doc__[] =
    "Set the value of a given extended attribute.\n"
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
    PyObject *myarg;
    int nofollow=0;
    char *attrname;
    char *buf;
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
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Oss#|iiz", kwlist,
                                     &myarg, &attrname,
                                     &buf, &bufsize, &flags, &nofollow, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    full_name = merge_ns(ns, attrname, &newname);
    /* Set the attribute's value */
    nret = _set_obj(&tgt, full_name, buf, bufsize, flags);
    if(newname != NULL)
        PyMem_Free(newname);
    if(nret == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Return the result */
    Py_RETURN_NONE;
}


static char __pyremovexattr_doc__[] =
    "Remove an attribute from a file (deprecated)\n"
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
    PyObject *myarg;
    int nofollow=0;
    char *attrname;
    int nret;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os|i", &myarg, &attrname, &nofollow))
        return NULL;

    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    /* Remove the attribute */
    if((nret = _remove_obj(&tgt, attrname)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Return the result */
    Py_RETURN_NONE;
}

static char __remove_doc__[] =
    "Remove an attribute from a file\n"
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
    PyObject *myarg;
    int nofollow=0;
    char *attrname, *name_buf;
    char *ns = NULL;
    const char *full_name;
    int nret;
    target_t tgt;
    static char *kwlist[] = {"item", "name", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Os|iz", kwlist,
                                     &myarg, &attrname, &nofollow, &ns))
        return NULL;

    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;
    full_name = merge_ns(ns, attrname, &name_buf);
    if(full_name == NULL)
        return NULL;

    /* Remove the attribute */
    nret = _remove_obj(&tgt, full_name);
    PyMem_Free(name_buf);
    if(nret == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Return the result */
    Py_RETURN_NONE;
}

static char __pylistxattr_doc__[] =
    "Return the list of attribute names for a file (deprecated)\n"
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
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the list of attributes */
    if((nret = _list_obj(&tgt, buf, nalloc)) == -1) {
        PyMem_Free(buf);
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Compute the number of attributes in the list */
    for(s = buf, nattrs = 0; (s - buf) < nret; s += strlen(s) + 1) {
        nattrs++;
    }

    /* Create the list which will hold the result */
    mylist = PyList_New(nattrs);

    /* Create and insert the attributes as strings in the list */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        PyList_SET_ITEM(mylist, nattrs, PyString_FromString(s));
        nattrs++;
    }

    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

    /* Return the result */
    return mylist;
}

static char __list_doc__[] =
    "Return the list of attribute names for a file\n"
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
    int nofollow=0;
    ssize_t nalloc, nret;
    PyObject *myarg;
    PyObject *mylist;
    char *ns = NULL;
    Py_ssize_t nattrs;
    char *s;
    target_t tgt;
    static char *kwlist[] = {"item", "nofollow", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|iz", kwlist,
                          &myarg, &nofollow, &ns))
        return NULL;
    if(!convertObj(myarg, &tgt, nofollow))
        return NULL;

    /* Find out the needed size of the buffer */
    if((nalloc = _list_obj(&tgt, NULL, 0)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the list of attributes */
    if((nret = _list_obj(&tgt, buf, nalloc)) == -1) {
        PyMem_Free(buf);
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Compute the number of attributes in the list */
    for(s = buf, nattrs = 0; (s - buf) < nret; s += strlen(s) + 1) {
        if(matches_ns(ns, s)!=NULL)
            nattrs++;
    }
    /* Create the list which will hold the result */
    mylist = PyList_New(nattrs);

    /* Create and insert the attributes as strings in the list */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        const char *name = matches_ns(ns, s);
        if(name!=NULL) {
            PyList_SET_ITEM(mylist, nattrs, PyString_FromString(name));
            nattrs++;
        }
    }

    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

    /* Return the result */
    return mylist;
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
    "Access extended filesystem attributes\n"
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

void
initxattr(void)
{
    PyObject *m = Py_InitModule3("xattr", xattr_methods, __xattr_doc__);

    PyModule_AddStringConstant(m, "__author__", _XATTR_AUTHOR);
    PyModule_AddStringConstant(m, "__version__", _XATTR_VERSION);
    PyModule_AddStringConstant(m, "__docformat__", "epytext en");

    PyModule_AddIntConstant(m, "XATTR_CREATE", XATTR_CREATE);
    PyModule_AddIntConstant(m, "XATTR_REPLACE", XATTR_REPLACE);

    /* namespace constants */
    PyModule_AddStringConstant(m, "NS_SECURITY", "security");
    PyModule_AddStringConstant(m, "NS_SYSTEM", "system");
    PyModule_AddStringConstant(m, "NS_TRUSTED", "trusted");
    PyModule_AddStringConstant(m, "NS_USER", "user");

}
