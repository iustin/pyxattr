#include <Python.h>
#include <attr/xattr.h>

static PyObject *
pygetxattr(PyObject *self, PyObject *args)
{
    char *file;
    char *attrname;
    char *buf;
    int nalloc, nret;
    PyObject *res;
    
    if (!PyArg_ParseTuple(args, "ss", &file, &attrname))
        return NULL;

    if((nalloc = getxattr(file, attrname, NULL, 0)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    if((nret = getxattr(file, attrname, buf, nalloc)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    res = PyString_FromStringAndSize(buf, nret);
    PyMem_Free(buf);
    return res;
}

static PyObject *
pysetxattr(PyObject *self, PyObject *args)
{
    char *file;
    char *attrname;
    char *buf;
    char mode = 0;
    int bufsize, nret;
    int flags;
    
    if (!PyArg_ParseTuple(args, "sss#|b", &file, &attrname, &buf, &bufsize, &mode))
        return NULL;

    flags = mode == 1 ? XATTR_CREATE : mode == 2 ? XATTR_REPLACE : 0;
    if((nret = setxattr(file, attrname, buf, bufsize, flags)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pyremovexattr(PyObject *self, PyObject *args)
{
    char *file;
    char *attrname;
    
    if (!PyArg_ParseTuple(args, "ss", &file, &attrname))
        return NULL;

    if(removexattr(file, attrname) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pylistxattr(PyObject *self, PyObject *args)
{
    char *file;
    char *buf;
    int nalloc, nret;
    PyObject *mytuple;
    int nattrs;
    char *s;
    
    /* Parse the arguments */
    if (!PyArg_ParseTuple(args, "s", &file))
        return NULL;

    /* Find out the needed size of the buffer */
    if((nalloc = listxattr(file, NULL, 0)) == -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    /* Try to allocate the memory, using Python's allocator */
    if((buf = PyMem_Malloc(nalloc)) == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Now retrieve the list of attributes */
    if((nret = listxattr(file, buf, nalloc)) == -1) {
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
