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
"\t      usually in form of system.posix_acl or user.mime_type" \
;

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    char *file;
    int filedes, ishandle;
    char *attrname;
    char *buf;
    int nalloc, nret;
    PyObject *res;
    
    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os", &myarg, &attrname))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Find out the needed size of the buffer */
    nalloc = ishandle ? 
        fgetxattr(filedes, attrname, NULL, 0) : 
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
        getxattr(file, attrname, buf, nalloc);
    if(nret == -1) {
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
"\t- a small integer; if ommited the attribute will be created\n" \
"\t      or replaced; if XATTR_CREATE, the attribute will be \n" \
"\t      created, giving an error if it already exists; if \n" \
"\t      XATTR_REPLACE, the attribute will be replaced, giving \n" \
"\t      an error if it doesn't exists." \
;

/* Wrapper for setxattr */
static PyObject *
pysetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    char *file;
    int ishandle, filedes;
    char *attrname;
    char *buf;
    int bufsize, nret;
    int flags = 0;
    
    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oss#|b", &myarg, &attrname, &buf, &bufsize, &flags))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Set the attribute's value */
    nret = ishandle ?
        fsetxattr(filedes, attrname, buf, bufsize, flags) :
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
"\t      usually in form of system.posix_acl or user.mime_type" \
;

/* Wrapper for removexattr */
static PyObject *
pyremovexattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    char *file;
    int ishandle, filedes;
    char *attrname;
    int nret;
    
    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Os", &myarg, &attrname))
        return NULL;

    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Remove the attribute */
    nret = ishandle ?
        fremovexattr(filedes, attrname) :
        removexattr(file, attrname);

    if(nret == -1)
        return PyErr_SetFromErrno(PyExc_IOError);

    /* Return the result */
    Py_INCREF(Py_None);
    return Py_None;
}

static char __pylistxattr_doc__[] = \
"Return the list of attribute names from a file\n" \
"\n" \
"Parameters:\n" \
"\t- a string representing filename, or a file-like object,\n" \
"\t      or a file descriptor; this represents the file to \n" \
"\t      be queried\n" \
;

/* Wrapper for listxattr */
static PyObject *
pylistxattr(PyObject *self, PyObject *args)
{
    char *file = NULL;
    int filedes = -1;
    char *buf;
    int ishandle;
    int nalloc, nret;
    PyObject *myarg;
    PyObject *mytuple;
    int nattrs;
    char *s;
    
    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "O", &myarg))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Find out the needed size of the buffer */
    nalloc = ishandle ?
        flistxattr(filedes, NULL, 0) :
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
;

void
initxattr(void)
{
    PyObject *m = Py_InitModule3("xattr", xattr_methods, __xattr_doc__);
    
    PyModule_AddIntConstant(m, "XATTR_CREATE", XATTR_CREATE);
    PyModule_AddIntConstant(m, "XATTR_REPLACE", XATTR_REPLACE);

}
