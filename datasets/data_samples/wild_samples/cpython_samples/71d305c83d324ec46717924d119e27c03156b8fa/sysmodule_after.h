extern "C" {
#endif

PyAPI_FUNC(PyObject *) PySys_GetObject(const char *);
PyAPI_FUNC(int) PySys_SetObject(const char *, PyObject *);
PyAPI_FUNC(void) PySys_SetArgv(int, wchar_t **);
PyAPI_FUNC(void) PySys_SetArgvEx(int, wchar_t **, int);
PyAPI_FUNC(void) PySys_SetPath(const wchar_t *);

PyAPI_FUNC(void) PySys_WriteStdout(const char *format, ...)
			Py_GCC_ATTRIBUTE((format(printf, 1, 2)));
PyAPI_FUNC(void) PySys_WriteStderr(const char *format, ...)
			Py_GCC_ATTRIBUTE((format(printf, 1, 2)));

PyAPI_DATA(PyObject *) _PySys_TraceFunc, *_PySys_ProfileFunc;
PyAPI_DATA(int) _PySys_CheckInterval;

PyAPI_FUNC(void) PySys_ResetWarnOptions(void);
PyAPI_FUNC(void) PySys_AddWarnOption(const wchar_t *);
PyAPI_FUNC(int) PySys_HasWarnOptions(void);

#ifdef __cplusplus
}
#endif
#endif /* !Py_SYSMODULE_H */