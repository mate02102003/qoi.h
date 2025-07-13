#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define QOI_IMPLEMENTATION
#include "../qoi.h"

static struct PyModuleDef qoipy_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "qoipy",
};

PyMODINIT_FUNC
PyInit_qoipy(void)
{
    return PyModule_Create(&qoipy_module);
}