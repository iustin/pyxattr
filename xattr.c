#include <Python.h>
#include <attr/xattr.h>

static int convertObj(PyObject *myobj, int *ishandle, int *filehandle, char **filename) {
    if(PyString_Check(myobj)) {
        *ishandle = 0;
        *filename = PyString_AS_STRING(myobj);
    } else if(PyInt_Check(myobj)) {
        *ishandle = 1;
        *filehandle = PyInt_AS_LONG(myobj);
    } else if(PyFile_Check(myobj)) {
        *ishandle = 1;
        *filehandle = fileno(PyFile_AsFile(myobj));
        if(*filehandle == -1) {
            PyErr_SetFromErrno(PyExc_IOError);
            return 0;
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be string or int");
        return 0;
    }
    return 1;
}

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    char *file;
    int filedes;
    int ishandle;
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

static PyObject *
pysetxattr(PyObject *self, PyObject *args)
{
    PyObject *myarg;
    char *file;
    int ishandle, filedes;
    char *attrname;
    char *buf;
    char mode = 0;
    int bufsize, nret;
    int flags;
    
    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "Oss#|b", &myarg, &attrname, &buf, &bufsize, &mode))
        return NULL;
    if(!convertObj(myarg, &ishandle, &filedes, &file))
        return NULL;

    /* Check the flags and convert them to libattr values */
    flags = mode == 1 ? XATTR_CREATE : mode == 2 ? XATTR_REPLACE : 0;

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

static PyMethodDef AclMethods[] = {
    {"getxattr",  pygetxattr, METH_VARARGS,
     "Get the value of a given extended attribute."},
    {"setxattr",  pysetxattr, METH_VARARGS,
     "Set the value of a given extended attribute."},
    {"removexattr",  pyremovexattr, METH_VARARGS,
     "Remove the a given extended attribute."},
    {"listxattr",  pylistxattr, METH_VARARGS,
     "Retrieve the list of extened attributes."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

void
initaclmodule(void)
{
    (void) Py_InitModule("aclmodule", AclMethods);
}
