
#include "alloc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "http.h"

ARRAY_DEFINE(Header, HeaderArray)



HttpRequest* http_request_new(void* tag) {
    HttpRequest* request = pmalloc(sizeof(HttpRequest), tag);
    if (!request) {
        printf("Failed to allocate memory for HttpRequest\n");
    }

    request->headers = pmalloc(sizeof(HeaderArray), tag);
    if (!request->headers) {
        printf("Failed to allocate memory for HeaderArray\n");
        pfree(request);
        return NULL;
    }

    request->request_line.method = HTTP_UNKNOWN;
    request->request_line.target = string_new_empty(tag);
    request->request_line.version = HTTP_UNKNOWN_VERSION;
    request->tag = tag;
    request->body = string_new_empty(tag);

    if (!HeaderArray_init(request->headers, tag)) {
        printf("Failed to initialize headers array\n");
        pfree(request);
        return NULL;
    }

    return request;
}

void http_request_free(HttpRequest* request) {
    if (!request) return;

    string_free(request->request_line.target);
    HeaderArray_destroy(request->headers);
    string_free(request->body);
    pfree(request);
}

void http_request_print(const HttpRequest* request) {
    if (!request) return;


    printf("Method: %s\n", http_request_method_to_string(request->request_line.method));
    printf("Target: %s\n", request->request_line.target->data);
    printf("Version: %d\n", request->request_line.version);

    for (size_t i = 0; i < HeaderArray_size(request->headers); i++) {
        Header* header = &request->headers->data[i];
        printf("Header: %s: %s\n", header->key->data, header->value->data);
    }

    printf("Body: %s\n", request->body->data);
}

bool http_request_parse(HttpRequest* request, String* raw) {
    if (!request || !raw) return false;

    void* tag = request->tag;

    // Split the raw request into lines
    String* line = string_new_empty(tag);
    size_t start = 0;
    size_t end = 0;

    while ((end = string_find_cstr(raw, "\r\n", start)) != SIZE_MAX) {
        line = string_substring(raw, start, end - start, tag);
        start = end + 2; // Move past the CRLF

        if (string_length(line) == 0) {
            break; // Empty line indicates end of headers
        }

        // Parse the request line
        if (request->request_line.method == HTTP_UNKNOWN) {
            // Parse the request line
            String* method_str = string_substring(line, 0, string_find_cstr(line, " ", 0), tag);
            if (string_equals_cstr(method_str, "GET")) {
                request->request_line.method = HTTP_GET;
            } else if (string_equals_cstr(method_str, "POST")) {
                request->request_line.method = HTTP_POST;
            } else if (string_equals_cstr(method_str, "PUT")) {
                request->request_line.method = HTTP_PUT;
            } else if (string_equals_cstr(method_str, "DELETE")) {
                request->request_line.method = HTTP_DELETE;
            } else if (string_equals_cstr(method_str, "PATCH")) {
                request->request_line.method = HTTP_PATCH;
            } else if (string_equals_cstr(method_str, "OPTIONS")) {
                request->request_line.method = HTTP_OPTIONS;
            } else if (string_equals_cstr(method_str, "HEAD")) {
                request->request_line.method = HTTP_HEAD;
            }

            // Parse the target
            //
            size_t target_start = string_find_cstr(line, " ", 0) + 1;
            size_t target_end = string_find_cstr(line, " ", target_start);
            request->request_line.target = string_substring(line, target_start, target_end - target_start, tag);

            // Parse the version
            size_t version_start = target_end + 1;
            if (string_equals_cstr(string_substring(line, version_start, string_length(line) - version_start, line->tag), "HTTP/1.1")) {
                request->request_line.version = HTTP_1_1;
            } else if (string_equals_cstr(string_substring(line, version_start, string_length(line) - version_start, line->tag), "HTTP/2.0")) {
                request->request_line.version = HTTP_2_0;
            }
        } else {
            // Parse headers
            size_t colon_pos = string_find_cstr(line, ": ", 0);
            if (colon_pos != SIZE_MAX) {
                String* key = string_substring(line, 0, colon_pos, tag);
                String* value = string_substring(line, colon_pos + 2, string_length(line) - colon_pos - 2, tag);

                Header header = { .key = key, .value = value };
                HeaderArray_push(request->headers, header);
            }
        }
        string_free(line);
        line = string_new_empty(tag);
    }

    if (start < string_length(raw)) {
        request->body = string_substring(raw, start, string_length(raw) - start, tag);
    }

    string_free(line);
    return true;
}

char* http_request_method_to_string(Method method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        case HTTP_OPTIONS: return "OPTIONS";
        case HTTP_HEAD: return "HEAD";
        default: return "UNKNOWN";
    }
}

Header* http_request_get_header(const HttpRequest* request, const char* key) {
    if (!request || !key) return NULL;

    for (size_t i = 0; i < HeaderArray_size(request->headers); i++) {
        Header* header = &request->headers->data[i];
        if (string_equals_cstr(header->key, key)) {
            return header;
        }
    }
    return NULL;
}

HttpResponse* http_response_new(void* tag) {
    HttpResponse* response = pmalloc(sizeof(HttpResponse), tag);
    if (!response) return NULL;
    response->headers = pmalloc(sizeof(HeaderArray), tag);
    if (!response->headers) {
        pfree(response);
        return NULL;
    }

    response->status = NULL;
    response->tag = tag;
    response->body = string_new_empty(tag);
    response->raw_body = NULL;

    if (!HeaderArray_init(response->headers, tag)) {
        pfree(response);
        return NULL;
    }

    return response;
}

void http_response_free(HttpResponse* response) {
    if (!response) return;

    HeaderArray_destroy(response->headers);
    string_free(response->body);
    if (response->raw_body) {
        // no not a mistake this will be allocated by zlib
        free(response->raw_body);
    }
    pfree(response);
}

void http_response_print(const HttpResponse* response) {
    if (!response) return;

    printf("Status Line: %s\n", response->status);
    for (size_t i = 0; i < HeaderArray_size(response->headers); i++) {
        Header* header = HeaderArray_get_ptr(response->headers, i);
        printf("Header: %s: %s\n", string_cstr(header->key), string_cstr(header->value));
    }
    printf("Body: %s\n", response->body->data);
}

bool http_response_send(const HttpResponse* response, int client_fd) {
    if (!response || client_fd < 0) {
        printf("Invalid response or client file descriptor\n");
        return false;
    }

    String* builder = string_new_empty(response->tag);

    // Build the response string
    string_append_cstr(builder, response->status);
    string_append_cstr(builder, "\r\n");

    // Add headers
    for (size_t i = 0; i < HeaderArray_size(response->headers); i++) {
        Header* header = HeaderArray_get_ptr(response->headers, i);
        string_append_cstr(builder, header->key->data);
        string_append_cstr(builder, ": ");
        string_append_cstr(builder, header->value->data);
        string_append_cstr(builder, "\r\n");
    }

    // Add a blank line to separate headers from the body
    string_append_cstr(builder, "\r\n");

    // Add the body
    if (response->encoding == COMPRESSION_NONE) {
        string_append(builder, response->body);
    } else if (response->encoding == COMPRESSION_GZIP) {
        // Send the compressed body
        printf("Sending raw body of length %zu\n", response->raw_body_len);
        string_append_bytes(builder, response->raw_body, response->raw_body_len);
    }

    // Send the response
    if (send(client_fd, builder->data, builder->byte_len, 0) == -1) {
        printf("Failed to send response: %s\n", strerror(errno));
        string_free(builder);
        return false;
    }

    string_free(builder);
    return true;
}

Header* http_response_get_header(const HttpResponse* request, const char* key) {
    if (!request || !key) return NULL;

    for (size_t i = 0; i < HeaderArray_size(request->headers); i++) {
        Header* header = &request->headers->data[i];
        if (string_equals_cstr(header->key, key)) {
            return header;
        }
    }
    return NULL;
}
