// Test-only interpreter: linking ASAN here initializes interceptors before Python
// loads native extensions, including when the platform Python launcher strips DYLD.
#include <Python.h>

int main(int argc, char** argv) { return Py_BytesMain(argc, argv); }
