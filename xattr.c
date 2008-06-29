#include <Python.h>
#include <attr/xattr.h>

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

static ssize_t _list_obj(target_t *tgt, char *list, size_t size) {
    if(tgt->type == T_FD)
        return flistxattr(tgt->fd, list, size);
    else if (tgt->type == T_LINK)
        return llistxattr(tgt->name, list, size);
    else
        return listxattr(tgt->name, list, size);
}

static ssize_t _get_obj(target_t *tgt, char *name, void *value, size_t size) {
    if(tgt->type == T_FD)
        return fgetxattr(tgt->fd, name, value, size);
    else if (tgt->type == T_LINK)
        return lgetxattr(tgt->name, name, value, size);
    else
        return getxattr(tgt->name, name, value, size);
}

static ssize_t _set_obj(target_t *tgt, char *name, void *value, size_t size,
                        int flags) {
    if(tgt->type == T_FD)
        return fsetxattr(tgt->fd, name, value, size, flags);
    else if (tgt->type == T_LINK)
        return lsetxattr(tgt->name, name, value, size, flags);
    else
        return setxattr(tgt->name, name, value, size, flags);
}

static ssize_t _remove_obj(target_t *tgt, char *name) {
    if(tgt->type == T_FD)
        return fremovexattr(tgt->fd, name);
    else if (tgt->type == T_LINK)
        return lremovexattr(tgt->name, name);
    else
        return removexattr(tgt->name, name);
}

/* Wrapper for getxattr */
static char __pygetxattr_doc__[] =
    "Get the value of a given extended attribute.\n"
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
    "    of its target;"
    ;

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    target_t tgt;
    int dolink=0;
    char *attrname;
    char *buf;
    int nalloc, nret;
    PyObject *res;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os|i", &myarg, &attrname, &dolink))
        return NULL;
    if(!convertObj(myarg, &tgt, dolink))
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

static char __pysetxattr_doc__[] =
    "Set the value of a given extended attribute.\n"
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
    "    of its target;"
    ;

/* Wrapper for setxattr */
static PyObject *
pysetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    int dolink=0;
    char *attrname;
    char *buf;
    int bufsize, nret;
    int flags = 0;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oss#|bi", &myarg, &attrname,
                          &buf, &bufsize, &flags, &dolink))
        return NULL;
    if(!convertObj(myarg, &tgt, dolink))
        return NULL;

    /* Set the attribute's value */
    if((nret = _set_obj(&tgt, attrname, buf, bufsize, flags)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Return the result */
    Py_INCREF(Py_None);
    return Py_None;
}

static char __pyremovexattr_doc__[] =
    "Remove an attribute from a file\n"
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
    ;

/* Wrapper for removexattr */
static PyObject *
pyremovexattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    int dolink=0;
    char *attrname;
    int nret;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os|i", &myarg, &attrname, &dolink))
        return NULL;

    if(!convertObj(myarg, &tgt, dolink))
        return NULL;

    /* Remove the attribute */
    if((nret = _remove_obj(&tgt, attrname)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Return the result */
    Py_INCREF(Py_None);
    return Py_None;
}

static char __pylistxattr_doc__[] =
    "Return the list of attribute names for a file\n"
    "\n"
    "Parameters:\n"
    "  - a string representing filename, or a file-like object,\n"
    "    or a file descriptor; this represents the file to \n"
    "    be queried\n"
    "  - (optional) a boolean value (defaults to false), which, if\n"
    "    the file name given is a symbolic link, makes the\n"
    "    function operate on the symbolic link itself instead\n"
    "    of its target;\n"
    ;

/* Wrapper for listxattr */
static PyObject *
pylistxattr(PyObject *self, PyObject *args)
{
    char *buf;
    int dolink=0;
    int nalloc, nret;
    PyObject *myarg;
    PyObject *mylist;
    int nattrs;
    char *s;
    target_t tgt;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "O|i", &myarg, &dolink))
        return NULL;
    if(!convertObj(myarg, &tgt, dolink))
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

static PyMethodDef xattr_methods[] = {
    {"getxattr",  pygetxattr, METH_VARARGS, __pygetxattr_doc__ },
    {"setxattr",  pysetxattr, METH_VARARGS, __pysetxattr_doc__ },
    {"removexattr",  pyremovexattr, METH_VARARGS, __pyremovexattr_doc__ },
    {"listxattr",  pylistxattr, METH_VARARGS, __pylistxattr_doc__ },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static char __xattr_doc__[] = \
    "Access extended filesystem attributes\n"
    "\n"
    "This module gives access to the extended attributes present\n"
    "in some operating systems/filesystems. You can list attributes,\n"
    "get, set and remove them.\n"
    "The last and optional parameter for all functions is a boolean \n"
    "value which enables the 'l-' version of the functions - acting\n"
    "on symbolic links and not their destination.\n"
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
    ""
    ;

void
initxattr(void)
{
    PyObject *m = Py_InitModule3("xattr", xattr_methods, __xattr_doc__);

    PyModule_AddIntConstant(m, "XATTR_CREATE", XATTR_CREATE);
    PyModule_AddIntConstant(m, "XATTR_REPLACE", XATTR_REPLACE);

    /* namespace constants */
    PyModule_AddStringConstant(m, "NS_SECURITY", "security");
    PyModule_AddStringConstant(m, "NS_SYSTEM", "system");
    PyModule_AddStringConstant(m, "NS_TRUSTED", "trusted");
    PyModule_AddStringConstant(m, "NS_USER", "user");

}
