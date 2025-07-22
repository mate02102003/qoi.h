#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stddef.h>

#define QOI_Malloc  PyMem_Malloc
#define QOI_Calloc  PyMem_Calloc
#define QOI_Realloc PyMem_Realloc
#define QOI_Free    PyMem_Free

#define QOI_IMPLEMENTATION
#include "../qoi.h"

typedef struct {
    PyObject_HEAD
    uint8_t r, g, b, a;
} PixelObject;

static int
Pixel_init(PixelObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = { "r", "g", "b", "a", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|bbbb", kwlist,
                                     &self->r,
                                     &self->g,
                                     &self->b,
                                     &self->a)
                                    )
        return -1;
    return 0;
}

static Py_hash_t
Pixel_hash(PixelObject *self) {
    return (self->r * 3 + self->g * 5 + self->b * 7 + self->a * 11) % 64;
}

static PyObject *
Pixel_repr(PixelObject *self) {
    if (Py_ReprEnter((PyObject*) self) != 0)
        return NULL;

    Py_ReprLeave((PyObject *) self);
    return PyUnicode_FromFormat("pixel(r=%d, g=%d, b=%d, a=%d)", self->r, self->g, self->b, self->a);
}

static PyMemberDef Pixel_members[] = {
    {"r", Py_T_UBYTE, offsetof(PixelObject, r), 0, ""},
    {"g", Py_T_UBYTE, offsetof(PixelObject, g), 0, ""},
    {"b", Py_T_UBYTE, offsetof(PixelObject, b), 0, ""},
    {"a", Py_T_UBYTE, offsetof(PixelObject, a), 0, ""},
    {NULL}  /* Sentinel */
};

static PyTypeObject PixelType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "qoipy.pixel",
    .tp_basicsize = sizeof(PixelObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) Pixel_init,
    .tp_members = Pixel_members,
    .tp_hash = (hashfunc) Pixel_hash,
    .tp_repr = (reprfunc) Pixel_repr,
};

typedef struct {
    PyObject_HEAD
    uint32_t     width;
    uint32_t     height;
    uint8_t      chanels;
    uint8_t      colorspace;
    PixelObject *pixels;
} QOIImageObject;

static PyTypeObject QOIImageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "qoipy.QOIImage",
    .tp_basicsize = sizeof(QOIImageObject),
    .tp_itemsize = sizeof(PixelObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
};

static struct PyModuleDef qoipy_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "qoipy",
    .m_doc = "Python bindings for header-only QOI format implementation",
};

PyMODINIT_FUNC
PyInit_qoipy(void)
{
    if (PyType_Ready(&PixelType) < 0)
        return NULL;
    if (PyType_Ready(&QOIImageType) < 0)
        return NULL;
    
    PyObject *m = PyModule_Create(&qoipy_module);
    if (m == NULL)
        return NULL;
    
    Py_INCREF(&PixelType);
    if (PyModule_AddObject(m, "pixel", (PyObject *) &PixelType) < 0) {
        Py_DECREF(&PixelType);
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(&QOIImageType);
    if (PyModule_AddObject(m, "QOIImage", (PyObject *) &QOIImageType) < 0) {
        Py_DECREF(&QOIImageType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}