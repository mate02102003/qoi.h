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

static int Pixel_init(PixelObject *self, PyObject *args, PyObject *kwargs) {
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

static Py_hash_t Pixel_hash(PixelObject *self) {
    return (self->r * 3 + self->g * 5 + self->b * 7 + self->a * 11) % 64;
}

static PyObject *Pixel_repr(PixelObject *self) {
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

typedef struct {
    PyObject_HEAD
    char          magic[4];
    uint32_t      width;
    uint32_t      height;
    uint8_t       chanels;
    uint8_t       colorspace;
    PixelObject **pixels;
} QOIImageObject;

static void QOIImage_dealloc(QOIImageObject *self) {
    uint64_t pixel_count = self->width * self->height;
    if (self->pixels != NULL) {
    for(uint64_t i = 0; i < pixel_count; ++i)
            if (self->pixels[i] != NULL)
        Py_XDECREF(self->pixels[i]);

    PyMem_Free(self->pixels);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *load_QOIImage (PyObject *Py_UNUSED(self), PyObject *arg);

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
    .tp_members   = QOIImage_members,
    .tp_methods   = QOIImage_methods,
};

bool copy_cimage_to_pyimage(QOIImageObject *py_image, qoi_image *c_image) {
    if (memcpy(py_image->magic, c_image->header.magic, sizeof(py_image->magic)) == NULL)
        return false;
    
    py_image->width      = c_image->header.width;
    py_image->height     = c_image->header.height;
    py_image->chanels    = c_image->header.chanels;
    py_image->colorspace = c_image->header.colorspace;

    uint64_t pixel_count = py_image->width * py_image->height;

    py_image->pixels = (PixelObject **)PyMem_Calloc(pixel_count, sizeof(PixelObject *));
    if (py_image->pixels == NULL)
        return false;

    for(uint64_t i = 0; i < pixel_count; ++i) {
        py_image->pixels[i] = PyObject_New(PixelObject, &PixelType);

        qoi_rgba c_pixel = c_image->image_data.items[i];

        if (Pixel_init(py_image->pixels[i], Py_BuildValue("bbbb", c_pixel.r, c_pixel.g, c_pixel.b, c_pixel.a), NULL) < 0)
            return false;
    }
    return true;
}

static PyObject *load_QOIImage(PyObject *Py_UNUSED(self), PyObject *arg) {
    const char *filepath;
    QOIImageObject *py_image = PyObject_New(QOIImageObject, &QOIImageType);
    qoi_image c_image = {0};
    
    if (!PyArg_Parse(arg, "s", &filepath))
        goto error;
    
    if (!qoi_load_image(filepath, &c_image))
        goto error;

    if (!copy_cimage_to_pyimage(py_image, &c_image))
        goto error;

    return (PyObject *)py_image;
error:
    Py_DECREF(py_image);
    qoi_free_image(&c_image);
    return NULL;
}

bool copy_pyimage_to_cimage(qoi_image *c_image, QOIImageObject *py_image) {
    if (memcpy(c_image->header.magic, py_image->magic, sizeof(py_image->magic)) == NULL)
        return false;
    
    c_image->header.width      = py_image->width;
    c_image->header.height     = py_image->height;
    c_image->header.chanels    = py_image->chanels;
    c_image->header.colorspace = py_image->colorspace;

    uint64_t pixel_count = c_image->header.width * c_image->header.height;

    for(uint64_t i = 0; i < pixel_count; ++i) {
        qoi_rgba c_pixel = {
            .r = py_image->pixels[i]->r,
            .g = py_image->pixels[i]->g,
            .b = py_image->pixels[i]->b,
            .a = py_image->pixels[i]->a,
        };
        qoi_da_append(&c_image->image_data, c_pixel);
    }
    return true;
}

static PyObject *write_QOIImage(QOIImageObject *self, PyObject *arg) {
    const char *filepath;
    qoi_image c_image = {0};

    if (!PyArg_Parse(arg, "s", &filepath))
        goto error;

    if (!copy_pyimage_to_cimage(&c_image, self))
        goto error;
        
    if (!qoi_write_image_from_qoi_image(filepath, c_image))
        goto error;

    Py_RETURN_NONE;
error:
    qoi_free_image(&c_image);
    return NULL;
}

static struct PyModuleDef qoipy_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "qoipy",
    .m_doc = "Python bindings for header-only QOI format implementation",
};

PyMODINIT_FUNC PyInit_qoipy(void)
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