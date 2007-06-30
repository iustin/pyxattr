#include <Python.h>
#include <attr/xattr.h>

/** Converts from a string, file or int argument to what we need. */
static int convertObj(PyObject *myobj, int *ishandle, int *filehandle, char **filename) {
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

/* Wrapper for getxattr */
static char __pygetxattr_doc__[] = \
"Get the value of a given extended attribute.\n" \
"\n" \
"Parameters:\n" \
"\t- a string representing filename, or a file-like object,\n" \
"\t      or a file descriptor; this represents the file on \n" \
"\t      which to act\n" \
"\t- a string, representing the attribute whose value to retrieve;\n" \
"\t      usually in form of system.posix_acl or user.mime_type\n" \
"\t- (optional) a boolean value (defaults to false), which, if\n" \
"\t      the file name given is a symbolic link, makes the\n" \
"\t      function operate on the symbolic link itself instead\n" \
"\t      of its target;" \
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

static char __pysetxattr_doc__[] = \
"Set the value of a given extended attribute.\n" \
"Be carefull in case you want to set attributes on symbolic\n" \
"links, you have to use all the 5 parameters; use 0 for the \n" \
"flags value if you want the default behavior (create or " \
"replace)\n" \
"\n" \
"Parameters:\n" \
"\t- a string representing filename, or a file-like object,\n" \
"\t      or a file descriptor; this represents the file on \n" \
"\t      which to act\n" \
"\t- a string, representing the attribute whose value to set;\n" \
"\t      usually in form of system.posix_acl or user.mime_type\n" \
"\t- a string, possibly with embedded NULLs; note that there\n" \
"\t      are restrictions regarding the size of the value, for\n" \
"\t      example, for ext2/ext3, maximum size is the block size\n" \
"\t- (optional) flags; if 0 or ommited the attribute will be \n" \
"\t      created or replaced; if XATTR_CREATE, the attribute \n" \
"\t      will be created, giving an error if it already exists;\n" \
"\t      of XATTR_REPLACE, the attribute will be replaced,\n" \
"\t      giving an error if it doesn't exists;\n" \
"\t- (optional) a boolean value (defaults to false), which, if\n" \
"\t      the file name given is a symbolic link, makes the\n" \
"\t      function operate on the symbolic link itself instead\n" \
"\t      of its target;" \
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
    if (!PyArg_ParseTuple(args, "Oss#|bi", &myarg, &attrname, &buf, &bufsize, &flags, &dolink))
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

static char __pyremovexattr_doc__[] = \
"Remove an attribute from a file\n" \
"\n" \
"Parameters:\n" \
"\t- a string representing filename, or a file-like object,\n" \
"\t      or a file descriptor; this represents the file on \n" \
"\t      which to act\n" \
"\t- a string, representing the attribute to be removed;\n" \
"\t      usually in form of system.posix_acl or user.mime_type\n" \
"\t- (optional) a boolean value (defaults to false), which, if\n" \
"\t      the file name given is a symbolic link, makes the\n" \
"\t      function operate on the symbolic link itself instead\n" \
"\t      of its target;" \
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

static char __pylistxattr_doc__[] = \
"Return the tuple of attribute names from a file\n" \
"\n" \
"Parameters:\n" \
"\t- a string representing filename, or a file-like object,\n" \
"\t      or a file descriptor; this represents the file to \n" \
"\t      be queried\n" \
"\t- (optional) a boolean value (defaults to false), which, if\n" \
"\t      the file name given is a symbolic link, makes the\n" \
"\t      function operate on the symbolic link itself instead\n" \
"\t      of its target;" \
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
    PyObject *mytuple;
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

    /* Create the tuple which will hold the result */
    mytuple = PyTuple_New(nattrs);

    /* Create and insert the attributes as strings in the tuple */
    for(s = buf, nattrs = 0; s - buf < nret; s += strlen(s) + 1) {
        PyTuple_SET_ITEM(mytuple, nattrs, PyString_FromString(s));
        nattrs++;
    }

    /* Free the buffer, now it is no longer needed */
    PyMem_Free(buf);

    /* Return the result */
    return mytuple;
}

static PyMethodDef xattr_methods[] = {
    {"getxattr",  pygetxattr, METH_VARARGS, __pygetxattr_doc__ },
    {"setxattr",  pysetxattr, METH_VARARGS, __pysetxattr_doc__ },
    {"removexattr",  pyremovexattr, METH_VARARGS, __pyremovexattr_doc__ },
    {"listxattr",  pylistxattr, METH_VARARGS, __pylistxattr_doc__ },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static char __xattr_doc__[] = \
"Access extended filesystem attributes\n" \
"\n" \
"This module gives access to the extended attributes present\n" \
"in some operating systems/filesystems. You can list attributes,\n"\
"get, set and remove them.\n"\
"The last and optional parameter for all functions is a boolean \n"\
"value which enables the 'l-' version of the functions - acting\n"\
"on symbolic links and not their destination.\n"\
"\n" \
"Example: \n" \
">>> import xattr\n" \
">>> xattr.listxattr(\"file.txt\")\n" \
"('user.mime_type',)\n" \
">>> xattr.getxattr(\"file.txt\", \"user.mime_type\")\n" \
"'text/plain'\n" \
">>> xattr.setxattr(\"file.txt\", \"user.comment\", \"Simple text file\")\n"\
">>> xattr.listxattr(\"file.txt\")\n" \
"('user.mime_type', 'user.comment')\n" \
">>> xattr.removexattr (\"file.txt\", \"user.comment\")\n" \
"" \
;

void
initxattr(void)
{
    PyObject *m = Py_InitModule3("xattr", xattr_methods, __xattr_doc__);

    PyModule_AddIntConstant(m, "XATTR_CREATE", XATTR_CREATE);
    PyModule_AddIntConstant(m, "XATTR_REPLACE", XATTR_REPLACE);

}
