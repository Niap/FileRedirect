#include <ntddk.h>

#define BUFFER_SIZE 1024

int wcsrep(wchar_t *str, const wchar_t *match, const wchar_t *rep);