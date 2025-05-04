#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include "cstring.h"

size_t gzip_string(String* str, uint8_t** out_ptr);

#endif
