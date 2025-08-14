#include "qoipy.h"

/*    PixelObject    */

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

/*    QOIImageObject    */

static int QOIImage_init(QOIImageObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = { "width", "height", "channels", "colorspace", "pixels", NULL };

    PyObject *pixels;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "IIbbO", kwlist, 
                                    &self->width,
                                    &self->height,
                                    &self->channels,
                                    &self->colorspace,
                                    &pixels)
                                    )
        goto error;

    if (memcpy(self->magic, "qoif", 4) == NULL)
        goto error;
    
    if (self->colorspace != 0 && self->colorspace != 1) {
        PyErr_Format(PyExc_ValueError, "colorspace is expected to be 0 or 1, but got (%u)!", self->colorspace);
        goto error;
    }

    if (self->channels != 3 && self->channels != 4) {
        PyErr_Format(PyExc_ValueError, "channels is expected to be 3 or 4, but got (%u)!", self->channels);
        goto error;
    }

    if (!PySequence_Check(pixels)) {
        PyErr_Format(PyExc_TypeError, "pixels must be a Sequence!");
        goto error;
    }

    Py_ssize_t seq_len = PySequence_Length(pixels);
    if (seq_len < 0) {
        PyErr_Format(PyExc_Exception, "couldn't get pixels length!");
        goto error;
    }

    uint64_t expected_len = self->width * self->height;
    if ((size_t)seq_len != expected_len) {
        PyErr_Format(PyExc_ValueError, "expected pixels length to be (%lu), but got (%d)!", expected_len, seq_len);
        goto error;
    }
    
    self->pixels = (PixelObject **)PyMem_Calloc(expected_len, sizeof(PixelObject *));
    if (self->pixels == NULL)
        goto error;

    for (Py_ssize_t i = 0; i < seq_len; ++i) {
        PyObject *pixel = PySequence_GetItem(pixels, i);

        if (!Py_IS_TYPE(pixel, &PixelType)) {
            PyErr_Format(PyExc_ValueError, "pixels elements must be qoipy.pixel!");
            goto error;
        }
        
        Py_INCREF(pixel);
        self->pixels[i] = (PixelObject *)pixel;
    }
    
    return 0;
error:
    Py_DECREF(&pixels);
    return -1;
}

static void QOIImage_dealloc(QOIImageObject *self) {
    uint64_t pixel_count = self->width * self->height;
    if (self->pixels != NULL) {
        for(uint64_t i = 0; i < pixel_count; ++i)
            Py_XDECREF(self->pixels[i]);
    
        PyMem_Free(self->pixels);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *QOIImage_repr(QOIImageObject *self) {
    if (Py_ReprEnter((PyObject*) self) != 0)
        return NULL;

    Py_ReprLeave((PyObject *) self);
    return PyUnicode_FromFormat("QOIImage(width=%u, height=%u, channels=%u, colorspace=%u)", self->width, self->height, self->channels, self->colorspace);
}

bool copy_cimage_to_pyimage(QOIImageObject *py_image, qoi_image *c_image) {
    if (memcpy(py_image->magic, c_image->header.magic, sizeof(py_image->magic)) == NULL)
        return false;
    
    py_image->width      = c_image->header.width;
    py_image->height     = c_image->header.height;
    py_image->channels   = c_image->header.channels;
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
    c_image->header.channels   = py_image->channels;
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

static PyObject *get_pixel_QOIImage(QOIImageObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = { "x", "y", NULL };

    uint32_t x, y;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "kk", kwlist, &x, &y))
        goto error;
    
    if (x > self->width) {
        PyErr_Format(PyExc_ValueError, "x must be between 0 and width (%k), 0 included!", self->width);
        goto error;
    }
    if (y > self->height) {
        PyErr_Format(PyExc_ValueError, "y must be between 0 and height (%k), 0 included!", self->height);
        goto error;
    }
    
    Py_INCREF(self->pixels[x + self->width * y]);
    return (PyObject *)self->pixels[x + self->width * y];
error:
    return NULL;
}

static PyObject *set_pixel_QOIImage(QOIImageObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = { "x", "y", "pixel", NULL };

    uint32_t x, y;
    PyObject *pixel;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "kkO", kwlist, &x, &y, &pixel))
        goto error;

    if (x > self->width) {
        PyErr_Format(PyExc_ValueError, "x must be between 0 and width (%k), 0 included!", self->width);
        goto error;
    }
    if (y > self->height) {
        PyErr_Format(PyExc_ValueError, "y must be between 0 and height (%k), 0 included!", self->height);
        goto error;
    }
    if (!Py_IS_TYPE(pixel, &PixelType)) {
        PyErr_Format(PyExc_TypeError, "pixel must be qoipy.pixel!");
        goto error;
    }

    self->pixels[x + self->width * y] = (PixelObject *)Py_NewRef(pixel);
    Py_RETURN_NONE;
error:
    return NULL;
}

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