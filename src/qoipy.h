#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stddef.h>

#define QOI_Malloc  PyMem_Malloc
#define QOI_Calloc  PyMem_Calloc
#define QOI_Realloc PyMem_Realloc
#define QOI_Free    PyMem_Free

#define QOI_IMPLEMENTATION
#include "../qoi.h"

/*    PixelObject    */

typedef struct {
    PyObject_HEAD
    uint8_t r, g, b, a;
} PixelObject;

static PyMemberDef Pixel_members[] = {
    {"r", Py_T_UBYTE, offsetof(PixelObject, r), 0, ""},
    {"g", Py_T_UBYTE, offsetof(PixelObject, g), 0, ""},
    {"b", Py_T_UBYTE, offsetof(PixelObject, b), 0, ""},
    {"a", Py_T_UBYTE, offsetof(PixelObject, a), 0, ""},
    {NULL}  /* Sentinel */
};

static int Pixel_init(PixelObject *self, PyObject *args, PyObject *kwargs);

static Py_hash_t Pixel_hash(PixelObject *self);

static PyObject *Pixel_repr(PixelObject *self);

static PyTypeObject PixelType = {
    .ob_base      = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "qoipy.pixel",
    .tp_basicsize = sizeof(PixelObject),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc) Pixel_init,
    .tp_members   = Pixel_members,
    .tp_hash      = (hashfunc) Pixel_hash,
    .tp_repr      = (reprfunc) Pixel_repr,
};

/*    QOIImageObject    */

typedef struct {
    PyObject_HEAD
    char          magic[4];
    uint32_t      width;
    uint32_t      height;
    uint8_t       chanels;
    uint8_t       colorspace;
    PixelObject **pixels;
} QOIImageObject;

static int QOIImage_init(QOIImageObject *self, PyObject *args, PyObject *kwargs);

static void QOIImage_dealloc(QOIImageObject *self);

static PyObject *QOIImage_repr(QOIImageObject *self);

static PyObject *load_QOIImage(PyObject *Py_UNUSED(self), PyObject *arg);

static PyObject *write_QOIImage(QOIImageObject *self, PyObject *arg);

static PyMemberDef QOIImage_members[] = {
    {"width",      Py_T_UINT , offsetof(QOIImageObject, width),      0, "Width of the image"},
    {"height",     Py_T_UINT , offsetof(QOIImageObject, height),     0, "Height of the image"},
    {"chanels",    Py_T_UBYTE, offsetof(QOIImageObject, chanels),    0, ""},
    {"colorspace", Py_T_UBYTE, offsetof(QOIImageObject, colorspace), 0, ""},
    {NULL}  /* Sentinel */
};

static PyMethodDef QOIImage_methods[] = {
    {"load" ,               load_QOIImage, METH_O | METH_STATIC, "Load QOI from file!"},
    {"write", (PyCFunction)write_QOIImage, METH_O              , "Write QOI to file!" },
    {NULL}  /* Sentinel */
};

static PyTypeObject QOIImageType = {
    .ob_base      = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "qoipy.QOIImage",
    .tp_basicsize = sizeof(QOIImageObject),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_init      = (initproc) QOIImage_init,
    .tp_dealloc   = (destructor) QOIImage_dealloc,
    .tp_repr      = (reprfunc) QOIImage_repr,
    .tp_members   = QOIImage_members,
    .tp_methods   = QOIImage_methods,
};

/*    qoipy_module    */

static struct PyModuleDef qoipy_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "qoipy",
    .m_doc = "Python bindings for header-only QOI format implementation",
};