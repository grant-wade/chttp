#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "utils.h"


// function to gzip String to use for the response
size_t gzip_string(String* str, uint8_t** out_ptr) {
    if (!str) return 0;

    z_stream z;
    int ret;

    // Initialize deflate stream with gzip format
    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;

    // 16+MAX_WBITS tells zlib to generate gzip format with header and footer
    ret = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        printf("Failed to initialize deflate: %d\n", ret);
        return 0;
    }

    // Set the gzip header mtime to current time to match gzip utility
    gz_header gzhead;
    memset(&gzhead, 0, sizeof(gzhead));

    gzhead.text = 0;
    gzhead.time = (uLong)time(NULL); // set mtime to current time
    gzhead.xflags = 0;
    gzhead.os = 255; // 255 = unknown, gzip uses 3 (Unix), but 255 is fine
    gzhead.extra = Z_NULL;
    gzhead.extra_len = 0;
    gzhead.extra_max = 0;
    gzhead.name = Z_NULL;
    gzhead.name_max = 0;
    gzhead.comment = Z_NULL;
    gzhead.comm_max = 0;
    gzhead.hcrc = 0;
    deflateSetHeader(&z, &gzhead);

    // Calculate buffer size needed
    size_t max_size = deflateBound(&z, str->byte_len);

    // Free the initial allocation if it exists
    if (*out_ptr) {
        free(*out_ptr);
    }

    // Allocate a new buffer of the proper size
    *out_ptr = (uint8_t*)malloc(max_size);
    if (!*out_ptr) {
        deflateEnd(&z);
        printf("Failed to allocate memory for compression\n");
        return 0;
    }

    // Setup input and output
    z.next_in = (Bytef*)str->data;
    z.avail_in = str->byte_len;
    z.next_out = *out_ptr;
    z.avail_out = max_size;

    // Compress all data in one go with Z_FINISH
    ret = deflate(&z, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&z);
        free(*out_ptr);
        *out_ptr = NULL;
        printf("Compression failed: %d\n", ret);
        return 0;
    }

    size_t compressed_size = z.total_out;
    deflateEnd(&z);

    return compressed_size;
}
