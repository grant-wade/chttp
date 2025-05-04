#include "builtin.h"
#include "alloc.h"
#include "utils.h"
#include <stdio.h>

bool logging_layer_preroute_verbose(HttpRequest* request, HttpResponse* response) {
    (void)response;
    printf("RECV: %s\n", string_cstr(request->request_line.target));
    http_request_print(request);
    return true;
}

bool logging_layer_postroute_verbose(HttpRequest* request, HttpResponse* response) {
    (void)request;
    printf("SENT: %s\n", response->status);
    http_response_print(response);
    return true;
}

// setup a logging LayerFns
bool logging_layer_preroute_basic(HttpRequest* request, HttpResponse* response) {
    (void)response;
    printf("RECV: %s\n", string_cstr(request->request_line.target));
    return true;
}

bool logging_layer_postroute_basic(HttpRequest* request, HttpResponse* response) {
    (void)request;
    printf("SENT: %s\n", response->status);
    return true;
}


bool content_encoding_layer(HttpRequest* request, HttpResponse* response) {
    void* tag = request->tag;
    Header* header = http_request_get_header(request, "Accept-Encoding");
    if (!header) {
        return false;
    }
    String* encoding;
    size_t index = 0;
    do { // loop through the encodings, we will get null after the last one or find what we want
        encoding = string_trim(string_isplit(header->value, ",", index++, tag), " ", tag);
    } while (!string_equals_cstr(encoding, "gzip") && encoding != NULL);

    if (encoding) {
        Header content_encoding_header = { .key = string_new("Content-Encoding", request->tag),
                                            .value = encoding };
        if (!HeaderArray_push(response->headers, content_encoding_header)) {
            printf("Failed to add Content-Encoding header\n");
            return false;
        }
        response->encoding = COMPRESSION_GZIP;
        // compress the body
        uint8_t* out = NULL;
        size_t compressed_size = gzip_string(response->body, &out);
        if (compressed_size == 0 || !out) {
            printf("Failed to compress response body\n");
            return false;
        }
        response->raw_body_len = compressed_size;
        response->raw_body = out;
        // set the content length header
        char content_length_str[32];
        snprintf(content_length_str, sizeof(content_length_str), "%zu", compressed_size);
        Header content_length_header = { .key = string_new("Content-Length", request->tag),
                                          .value = string_new(content_length_str, request->tag) };
        if (!HeaderArray_push(response->headers, content_length_header)) {
            printf("Failed to add Content-Length header\n");
            return false;
        }

        return true;
    }
    return false;
}

bool content_length_layer(HttpRequest* request, HttpResponse* response) {
    (void)request;
    if (response->encoding == COMPRESSION_GZIP) {
        // already set in the content encoding layer
        return true;
    }
    // set the content length header
    char content_length_str[32];
    snprintf(content_length_str, sizeof(content_length_str), "%zu", string_byte_length(response->body));
    Header content_length_header = { .key = string_new("Content-Length", request->tag),
                                      .value = string_new(content_length_str, request->tag) };
    if (!HeaderArray_push(response->headers, content_length_header)) {
        printf("Failed to add Content-Length header\n");
        return false;
    }
    return true;
}

bool connection_close_layer(HttpRequest* request, HttpResponse* response) {
    Header* connection_header = http_request_get_header(request, "Connection");
    if (connection_header && string_equals_cstr(connection_header->value, "close")) {
        // set the Connection: close header
        Header connection_close_header = { .key = string_new("Connection", request->tag),
                                            .value = string_new("close", request->tag) };
        if (!HeaderArray_push(response->headers, connection_close_header)) {
            printf("Failed to add Connection: close header\n");
            return false;
        }
    }
    return true;
}

bool request_memory_usage_layer(HttpRequest* request, HttpResponse* response) {
    (void)response;
    size_t total_bytes = ptag_size(request->tag);
    if (total_bytes < 1024) {
        printf("MEM: %zu bytes\n", total_bytes);
    } else if (total_bytes < 1024 * 1024) {
        printf("MEM: %.2f KB\n", total_bytes / 1024.0);
    } else {
        printf("MEM: %.2f MB\n", total_bytes / (1024.0 * 1024.0));
    }
    return true;
}
