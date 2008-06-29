#include <Python.h>
#include <attr/xattr.h>

/** Converts from a string, file or int argument to what we need. */
static int convertObj(PyObject *myobj, int *ishandle, int *filehandle,
                      char **filename) {
    if(PyString_Check(myobj)) {
        *ishandle = 0;
        *filename = PyString_AS_STRING(myobj);
    } else if((*filehandle = PyObject_AsFileDescriptor(myobj)) != -1) {
        *ishandle = 1;
    } else {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be string or int");
        return 0;
    }
    return 1;
}

/* Checks if an attribute name matches an optional namespace */
static int matches_ns(const char *name, const char *ns) {
    size_t ns_size;
    if (ns == NULL)
        return 1;
    ns_size = strlen(ns);

    if (strlen(name) > ns_size && !strncmp(name, ns, ns_size) &&
        name[ns_size] == '.')
        return 1;
    return 0;
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
    "    of its target;\n"
    "@deprecated: this function has been replace with the L{get_all} function"
    " which replaces the positional parameters with keyword ones\n"
    ;

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    char *file = NULL;
    int filedes = -1, ishandle, dolink=0;
    char *attrname;
    char *buf;
    int nalloc, nret;
    PyObject *res;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os|i", &myarg, &attrname, &dolink))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Find out the needed size of the buffer */
    nalloc = ishandle ?
        fgetxattr(filedes, attrname, NULL, 0) :
        dolink ?
        lgetxattr(file, attrname, NULL, 0) :
        getxattr(file, attrname, NULL, 0);
    if(nalloc == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the attribute value */
    nret = ishandle ?
        fgetxattr(filedes, attrname, buf, nalloc) :
        dolink ?
        lgetxattr(file, attrname, buf, nalloc) :
        getxattr(file, attrname, buf, nalloc);
    if(nret == -1) {
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
static char __get_all_doc__[] =
    "Get all the extended attributes of an item.\n"
    "\n"
    "This function performs a bulk-get of all extended attribute names\n"
    "and the corresponding value.\n"
    "Example:\n"
    "  >>> xattr.get_all('/path/to/file')\n"
    "  [('user.mime-type', 'plain/text'), ('user.comment', 'test'),"
    " ('system.posix_acl_access', '\\x02\\x00...')]\n"
    "  >>> xattr.get_all('/path/to/file', namespace=xattr.NS_USER)\n"
    "  [('user.mime-type', 'plain/text'), ('user.comment', 'test')]\n"
    "\n"
    "@param item: the item to query; either a string representing the"
    " filename, or a file-like object, or a file descriptor\n"
    "@keyword namespace: an optional namespace for filtering the"
    " attributes; for example, querying all user attributes can be"
    " accomplished by passing namespace=L{NS_USER}\n"
    "@type namespace: string\n"
    "@keyword noderef: if passed and true, if the target file is a symbolic"
    " link,"
    " the attributes for the link itself will be returned, instead of the"
    " attributes of the target\n"
    "@type noderef: boolean\n"
    "@return: list of tuples (name, value)\n"
    ;

static PyObject *
get_all(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *myarg;
    char *file = NULL;
    int filedes = -1, ishandle, dolink=0;
    char *ns = NULL;
    char *buf_list, *buf_val;
    char *s;
    size_t nalloc, nlist, nval;
    PyObject *mylist;
    static char *kwlist[] = {"item", "noderef", "namespace", NULL};

    /* Parse the arguments */
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|iz", kwlist,
                                     &myarg, &dolink, &ns))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Compute first the list of attributes */

    /* Find out the needed size of the buffer for the attribute list */
    nalloc = ishandle ?
        flistxattr(filedes, NULL, 0) :
        dolink ?
        llistxattr(file, NULL, 0) :
        listxattr(file, NULL, 0);

    if(nalloc == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf_list = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the list of attributes */
    nlist = ishandle ?
        flistxattr(filedes, buf_list, nalloc) :
        dolink ?
        llistxattr(file, buf_list, nalloc) :
        listxattr(file, buf_list, nalloc);

    if(nlist == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Create the list which will hold the result */
    mylist = PyList_New(0);
    nalloc = 256;
    if((buf_val = PyMem_Malloc(nalloc)) == NULL) {
        PyMem_Free(buf_list);
        PyErr_NoMemory();
        return NULL;
    }

    /* Create and insert the attributes as strings in the list */
    for(s = buf_list; s - buf_list < nlist; s += strlen(s) + 1) {
        PyObject *my_tuple;
        int missing;

        if(!matches_ns(s, ns))
            continue;
        /* Now retrieve the attribute value */
        missing = 0;
        while(1) {
            nval = ishandle ?
                fgetxattr(filedes, s, buf_val, nalloc) :
                dolink ?
                lgetxattr(file, s, buf_val, nalloc) :
                getxattr(file, s, buf_val, nalloc);

            if(nval == -1) {
                if(errno == ERANGE) {
                    nval = ishandle ?
                        fgetxattr(filedes, s, NULL, 0) :
                        dolink ?
                        lgetxattr(file, s, NULL, 0) :
                        getxattr(file, s, NULL, 0);
                    if((buf_val = PyMem_Realloc(buf_val, nval)) == NULL) {
                        PyMem_Free(buf_list);
                        PyErr_NoMemory();
                        return NULL;
                    }
                    nalloc = nval;
                    continue;
                } else if(errno == ENODATA || errno == ENOATTR) {
                    /* this attribute has gone away since we queried
                       the attribute list */
                    missing = 1;
                    break;
                }
                PyMem_Free(buf_list);
                PyMem_Free(buf_val);
                return PyErr_SetFromErrno(PyExc_IOError);
            }
            break;
        }
        if(missing)
            continue;
        my_tuple = Py_BuildValue("ss#", s, buf_val, nval);

        PyList_Append(mylist, my_tuple);
        Py_DECREF(my_tuple);
    }

    /* Free the buffers, now they are no longer needed */
    PyMem_Free(buf_val);
    PyMem_Free(buf_list);

    /* Return the result */
    return mylist;

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
    char *file;
    int ishandle, filedes, dolink=0;
    char *attrname;
    char *buf;
    int bufsize, nret;
    int flags = 0;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oss#|bi", &myarg, &attrname,
                          &buf, &bufsize, &flags, &dolink))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Set the attribute's value */
    nret = ishandle ?
        fsetxattr(filedes, attrname, buf, bufsize, flags) :
        dolink ?
        lsetxattr(file, attrname, buf, bufsize, flags) :
        setxattr(file, attrname, buf, bufsize, flags);

    if(nret == -1) {
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
    char *file;
    int ishandle, filedes, dolink=0;
    char *attrname;
    int nret;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os|i", &myarg, &attrname, &dolink))
        return NULL;

    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Remove the attribute */
    nret = ishandle ?
        fremovexattr(filedes, attrname) :
        dolink ?
        lremovexattr(file, attrname) :
        removexattr(file, attrname);

    if(nret == -1)
        return PyErr_SetFromErrno(PyExc_IOError);

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
    char *file = NULL;
    int filedes = -1;
    char *buf;
    int ishandle, dolink=0;
    int nalloc, nret;
    PyObject *myarg;
    PyObject *mylist;
    int nattrs;
    char *s;

    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "O|i", &myarg, &dolink))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Find out the needed size of the buffer */
    nalloc = ishandle ?
        flistxattr(filedes, NULL, 0) :
        dolink ?
        llistxattr(file, NULL, 0) :
        listxattr(file, NULL, 0);

    if(nalloc == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the list of attributes */
    nret = ishandle ?
        flistxattr(filedes, buf, nalloc) :
        dolink ?
        llistxattr(file, buf, nalloc) :
        listxattr(file, buf, nalloc);

    if(nret == -1) {
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
    {"get_all", (PyCFunction) get_all, METH_VARARGS | METH_KEYWORDS,
     __get_all_doc__ },
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
