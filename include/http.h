
#ifndef HTTP_H
#define HTTP_H

#include "array.h"
#include "cstring.h"
#include <sys/socket.h>

typedef enum {
    HTTP_UNKNOWN = 0,
    HTTP_GET = 1,
    HTTP_POST = 2,
    HTTP_PUT = 4,
    HTTP_DELETE = 8,
    HTTP_PATCH = 16,
    HTTP_OPTIONS = 32,
    HTTP_HEAD = 64,
} Method;

typedef uint32_t Methods;

#define IN_METHODS(methods, method) \
    ((methods & method) == method)

typedef enum {
    HTTP_1_0,
    HTTP_1_1,
    HTTP_2_0,
    HTTP_UNKNOWN_VERSION
} HttpVersion;

typedef enum {
    COMPRESSION_NONE,
    COMPRESSION_GZIP,
} Encoding;

typedef struct {
    Method method;
    String* target;
    HttpVersion version;
} RequestLine;

typedef struct {
    String* key;
    String* value;
} Header;

ARRAY_DECLARE(Header, HeaderArray)

typedef struct {
    RequestLine request_line;
    HeaderArray* headers;
    String* body;
    void* tag;
} HttpRequest;

typedef struct {
    char* status;
    HeaderArray* headers;
    Encoding encoding;
    String* body;
    uint8_t* raw_body;
    size_t raw_body_len;
    void* tag;
} HttpResponse;

HttpRequest* http_request_new(void* tag);
void http_request_free(HttpRequest* request);
void http_request_print(const HttpRequest* request);
bool http_request_parse(HttpRequest* request, String* raw);
char* http_request_method_to_string(Method method);
Header* http_request_get_header(const HttpRequest* request, const char* key);

HttpResponse* http_response_new(void* tag);
void http_response_free(HttpResponse* response);
void http_response_print(const HttpResponse* response);
bool http_response_send(const HttpResponse* response, int client_fd);
Header* http_response_get_header(const HttpResponse* request, const char* key);

#endif
