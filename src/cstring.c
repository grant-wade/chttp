#include "cstring.h"
#include "alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// Helper function to calculate UTF-8 characters count
static size_t count_utf8_chars(const char* str, size_t byte_len) {
    if (!str || byte_len == 0) return 0;

    size_t count = 0;
    size_t i = 0;

    while (i < byte_len) {
        unsigned char c = (unsigned char)str[i];

        // Count a character for each starting byte
        // (not a continuation byte 10xxxxxx)
        if ((c & 0xC0) != 0x80) {
            count++;
        }

        // Move to next character byte(s)
        size_t char_byte_len = string_utf8_char_len(str + i);
        if (char_byte_len == 0) { // Should not happen if byte_len > 0
             break;
        }
        // Ensure we don't read past the end if the last char is incomplete
        if (i + char_byte_len > byte_len) {
            // If the first byte indicates a multi-byte char but not enough bytes remain,
            // count it if it wasn't a continuation byte.
            if ((c & 0xC0) != 0x80) {
                 // Already counted above.
            }
             break; // Stop processing
        }
        i += char_byte_len;
    }

    return count;
}

// Helper function for increasing string capacity
static bool string_ensure_capacity(String* str, size_t min_capacity) {
    if (str->capacity >= min_capacity) return true;

    // Calculate new capacity
    size_t new_capacity = str->capacity;
    while (new_capacity < min_capacity) {
        new_capacity *= STRING_GROWTH_FACTOR;
        if (new_capacity == 0) new_capacity = STRING_INITIAL_CAPACITY;
    }

    // Reallocate memory
    char* new_data = prealloc(str->data, new_capacity, str->tag);
    if (!new_data) return false;

    str->data = new_data;
    str->capacity = new_capacity;
    return true;
}

// Create a new string from a C string
String* string_new(const char* cstr, void* tag) {
    if (!cstr) return string_new_empty(tag);
    return string_new_len(cstr, strlen(cstr), tag);
}

// Create a new string with specified length
String* string_new_len(const char* str, size_t len, void* tag) {
    // Allocate string struct
    String* s = pmalloc(sizeof(String), tag);
    if (!s) return NULL;

    // Calculate capacity (at least len + 1 for null terminator)
    size_t capacity = len + 1;
    if (capacity < STRING_INITIAL_CAPACITY) {
        capacity = STRING_INITIAL_CAPACITY;
    }

    // Allocate data buffer using the provided tag
    s->data = pmalloc(capacity, tag); // Use the passed 'tag' here
    if (!s->data) {
        pfree(s); // Free the string struct if data allocation fails
        return NULL;
    }

    s->capacity = capacity;
    s->byte_len = len;
    s->tag = tag; // Store the tag in the string struct as well

    // Copy string data if provided
    if (str && len > 0) {
        memcpy(s->data, str, len);
    }

    // Null terminate
    s->data[len] = '\0';

    // Count UTF-8 characters
    s->char_len = count_utf8_chars(s->data, len);

    return s;
}

// Create an empty string
String* string_new_empty(void* tag) {
    return string_new_len(NULL, 0, tag);
}

String* string_new_from_owned(const char* str, size_t len, void* tag) {
    // Allocate string struct
    String* s = pmalloc(sizeof(String), tag);
    if (!s) return NULL;

    // Set the data pointer to the provided buffer
    s->data = (char*)str;  // Take ownership of the buffer
    s->capacity = len;
    s->byte_len = len;
    s->tag = tag; // Store the tag in the string struct as well

    // Null terminate
    s->data[len] = '\0';

    // Count UTF-8 characters
    s->char_len = count_utf8_chars(s->data, len);

    return s;
}

// Create a copy of another string
String* string_copy(const String* other, void* tag) {
    if (!other) return NULL;
    return string_new_len(other->data, other->byte_len, tag);
}

// Free a string and its data
void string_free(String* str) {
    if (!str) return;

    if (str->data) {
        pfree(str->data);
    }

    pfree(str);
}

// Get number of UTF-8 characters
size_t string_length(const String* str) {
    if (!str) return 0;
    return str->char_len;
}

// Get number of bytes
size_t string_byte_length(const String* str) {
    if (!str) return 0;
    return str->byte_len;
}

// Get raw bytes (not null-terminated)
char* string_bytes(const String* str) {
    if (!str) return NULL;
    return str->data;
}

// Get null-terminated C string
const char* string_cstr(const String* str) {
    if (!str) return NULL;
    return str->data;  // Already null-terminated
}

// Check if two strings are equal
bool string_equals(const String* str1, const String* str2) {
    if (str1 == str2) return true;
    if (!str1 || !str2) return false;

    if (str1->byte_len != str2->byte_len) return false;
    return memcmp(str1->data, str2->data, str1->byte_len) == 0;
}

// Check if a string equals a C string
bool string_equals_cstr(const String* str, const char* cstr) {
    if (!str || !cstr) return false;

    size_t cstr_len = strlen(cstr);
    if (str->byte_len != cstr_len) return false;

    return memcmp(str->data, cstr, cstr_len) == 0;
}

// Compare two strings lexicographically
int string_compare(const String* str1, const String* str2) {
    if (str1 == str2) return 0;
    if (!str1) return -1;
    if (!str2) return 1;

    size_t min_len = str1->byte_len < str2->byte_len ? str1->byte_len : str2->byte_len;
    int result = memcmp(str1->data, str2->data, min_len);

    if (result != 0) return result;

    // If common prefix is the same, shorter string comes first
    if (str1->byte_len < str2->byte_len) return -1;
    if (str1->byte_len > str2->byte_len) return 1;
    return 0;
}

// Append one string to another
void string_append(String* str, const String* other) {
    if (!str || !other || other->byte_len == 0) return;

    string_append_cstr(str, other->data);
}

// Append a C string
void string_append_cstr(String* str, const char* cstr) {
    if (!str || !cstr || *cstr == '\0') return;

    size_t cstr_len = strlen(cstr);
    size_t new_size = str->byte_len + cstr_len;

    // Ensure we have enough space
    if (!string_ensure_capacity(str, new_size + 1)) return;  // +1 for null terminator

    // Append the string
    memcpy(str->data + str->byte_len, cstr, cstr_len);
    str->byte_len = new_size;
    str->data[new_size] = '\0';

    // Update character count
    str->char_len = count_utf8_chars(str->data, str->byte_len);
}

// Check if a byte sequence is a valid UTF-8 string
bool string_is_valid_utf8(const char* str, size_t len) {
    if (!str) return false;

    const unsigned char* bytes = (const unsigned char*)str;
    size_t i = 0;

    while (i < len) {
        if (bytes[i] <= 0x7F) {
            // ASCII character
            i += 1;
        } else if ((bytes[i] & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= len) return false;  // Incomplete sequence
            if ((bytes[i+1] & 0xC0) != 0x80) return false;  // Invalid continuation

            // Check for overlong encoding
            if ((bytes[i] & 0x1E) == 0) return false;

            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= len) return false;  // Incomplete sequence
            if ((bytes[i+1] & 0xC0) != 0x80 || (bytes[i+2] & 0xC0) != 0x80) return false;

            // Check for overlong encoding
            if ((bytes[i] & 0x0F) == 0 && (bytes[i+1] & 0x20) == 0) return false;

            // Check for UTF-16 surrogates (illegal in UTF-8)
            unsigned int code_point = ((bytes[i] & 0x0F) << 12) |
                                     ((bytes[i+1] & 0x3F) << 6) |
                                      (bytes[i+2] & 0x3F);
            if (code_point >= 0xD800 && code_point <= 0xDFFF) return false;

            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= len) return false;  // Incomplete sequence
            if ((bytes[i+1] & 0xC0) != 0x80 || (bytes[i+2] & 0xC0) != 0x80 ||
                (bytes[i+3] & 0xC0) != 0x80) return false;

            // Check for overlong encoding
            if ((bytes[i] & 0x07) == 0 && (bytes[i+1] & 0x30) == 0) return false;

            // Check valid range (0x10000 - 0x10FFFF)
            unsigned int code_point = ((bytes[i] & 0x07) << 18) |
                                     ((bytes[i+1] & 0x3F) << 12) |
                                     ((bytes[i+2] & 0x3F) << 6) |
                                      (bytes[i+3] & 0x3F);
            if (code_point > 0x10FFFF) return false;

            i += 4;
        } else {
            // Invalid UTF-8 lead byte
            return false;
        }
    }

    return true;
}

// Get the number of bytes in the current UTF-8 character
size_t string_utf8_char_len(const char* str) {
    if (!str || !*str) return 0;

    unsigned char c = (unsigned char)*str;

    if ((c & 0x80) == 0) return 1;       // ASCII
    if ((c & 0xE0) == 0xC0) return 2;    // 2-byte sequence
    if ((c & 0xF0) == 0xE0) return 3;    // 3-byte sequence
    if ((c & 0xF8) == 0xF0) return 4;    // 4-byte sequence

    return 1;  // Invalid UTF-8, treat as single byte
}

// Decode a single UTF-8 character to a Unicode code point
rune string_decode_utf8_char(const char* str, size_t* bytes_read) {
    if (!str || !*str) {
        if (bytes_read) *bytes_read = 0;
        return 0;
    }

    const unsigned char* bytes = (const unsigned char*)str;
    rune codepoint = 0;
    size_t len = 0;

    if ((bytes[0] & 0x80) == 0) {
        // ASCII
        codepoint = bytes[0];
        len = 1;
    } else if ((bytes[0] & 0xE0) == 0xC0) {
        // 2-byte sequence
        codepoint = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
        len = 2;
    } else if ((bytes[0] & 0xF0) == 0xE0) {
        // 3-byte sequence
        codepoint = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
        len = 3;
    } else if ((bytes[0] & 0xF8) == 0xF0) {
        // 4-byte sequence
        codepoint = ((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) |
                   ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
        len = 4;
    } else {
        // Invalid UTF-8, return replacement character
        codepoint = 0xFFFD;  // Unicode replacement character
        len = 1;
    }

    if (bytes_read) *bytes_read = len;
    return codepoint;
}

// Encode a Unicode code point to UTF-8
size_t string_encode_utf8_char(rune codepoint, char* buffer) {
    if (!buffer) return 0;

    if (codepoint <= 0x7F) {
        // ASCII - 1 byte
        buffer[0] = (char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        // 2 bytes
        buffer[0] = (char)(0xC0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint <= 0xFFFF) {
        // 3 bytes
        buffer[0] = (char)(0xE0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        // 4 bytes
        buffer[0] = (char)(0xF0 | (codepoint >> 18));
        buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }

    // Invalid code point, use replacement character
    buffer[0] = (char)0xEF;
    buffer[1] = (char)0xBF;
    buffer[2] = (char)0xBD;
    return 3;
}

// Append a single Unicode character (code point)
void string_append_char(String* str, rune codepoint) {
    if (!str) return;

    char buffer[5] = {0};  // Max 4 bytes for UTF-8 + null
    size_t bytes = string_encode_utf8_char(codepoint, buffer);

    // Ensure capacity
    if (!string_ensure_capacity(str, str->byte_len + bytes + 1)) return;

    // Append the encoded character
    memcpy(str->data + str->byte_len, buffer, bytes);
    str->byte_len += bytes;
    str->data[str->byte_len] = '\0';

    // Increment character count
    str->char_len++;
}

void string_append_bytes(String* str, const uint8_t* bytes, size_t len) {
    if (!str || !bytes || len == 0) return;

    // Ensure we have enough space
    if (!string_ensure_capacity(str, str->byte_len + len + 1)) return;  // +1 for null terminator

    // Append the bytes
    memcpy(str->data + str->byte_len, bytes, len);
    str->byte_len += len;
    str->data[str->byte_len] = '\0';

    // Update character count
    str->char_len = count_utf8_chars(str->data, str->byte_len);
}

// Create a substring
String* string_substring(const String* str, size_t start, size_t length, void* tag) {
    if (!str) return NULL;
    if (start >= str->char_len) return string_new_empty(tag);

    // Convert character indices to byte indices
    size_t start_byte = string_char_index_to_byte(str, start);

    // Calculate end character position (capped at string length)
    size_t end = start + length;
    if (end > str->char_len) end = str->char_len;

    // Convert end character position to byte index
    size_t end_byte = string_char_index_to_byte(str, end);

    // Create the substring
    return string_new_len(str->data + start_byte, end_byte - start_byte, tag);
}

uint64_t string_hash(const String* str) {
    if (!str) return 0;

    uint64_t hash = 5381;
    for (size_t i = 0; i < str->byte_len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)str->data[i]; // hash * 33 + c
    }

    return hash;
}

// Clear a string, making it empty
void string_clear(String* str) {
    if (!str) return;

    str->byte_len = 0;
    str->char_len = 0;
    if (str->data) str->data[0] = '\0';
}

// Get a specific UTF-8 character as a Unicode code point
rune string_char_at(const String* str, size_t index) {
    if (!str || index >= str->char_len) return 0;

    // Find the byte position of the character
    size_t byte_pos = string_char_index_to_byte(str, index);
    if (byte_pos >= str->byte_len) return 0;

    // Decode the character
    return string_decode_utf8_char(str->data + byte_pos, NULL);
}

// Convert character index to byte index
size_t string_char_index_to_byte(const String* str, size_t char_index) {
    if (!str || char_index >= str->char_len) return str ? str->byte_len : 0;

    const unsigned char* bytes = (const unsigned char*)str->data;
    size_t byte_index = 0;
    size_t char_count = 0;

    while (byte_index < str->byte_len && char_count < char_index) {
        // Skip continuation bytes (10xxxxxx)
        if ((bytes[byte_index] & 0xC0) != 0x80) {
            char_count++;
        }
        byte_index++;
    }

    // Make sure we're at the start of a character
    while (byte_index < str->byte_len && (bytes[byte_index] & 0xC0) == 0x80) {
        byte_index++;
    }

    return byte_index;
}

// Convert byte index to character index
size_t string_byte_index_to_char(const String* str, size_t byte_index) {
    if (!str || byte_index >= str->byte_len) return str ? str->char_len : 0;

    const unsigned char* bytes = (const unsigned char*)str->data;
    size_t char_count = 0;

    for (size_t i = 0; i < byte_index; i++) {
        // Count only non-continuation bytes
        if ((bytes[i] & 0xC0) != 0x80) {
            char_count++;
        }
    }

    return char_count;
}

// Find substring within string, returning character index
size_t string_find(const String* str, const String* substr, size_t start_pos) {
    if (!str || !substr) return -1;

    // Special case: empty substring always matches at position 0
    if (substr->byte_len == 0) return 0;

    return string_find_cstr(str, substr->data, start_pos);
}

// Find C string within string, returning character index
size_t string_find_cstr(const String* str, const char* substr, size_t start_pos) {
    if (!str || !substr) return -1;

    // Special case: empty substring always matches at position 0
    if (*substr == '\0') return 0;

    if (start_pos >= str->char_len) return -1;

    // Convert start position to byte index
    const size_t start_byte = string_char_index_to_byte(str, start_pos);
    const size_t substr_len = strlen(substr);

    if (substr_len > (str->byte_len - start_byte)) return -1;

    // Simple search implementation
    // Could be optimized with KMP or Boyer-Moore algorithm for production use
    for (size_t i = start_byte; i <= str->byte_len - substr_len; i++) {
        if (memcmp(str->data + i, substr, substr_len) == 0) {
            // Found match, convert byte index back to character index
            return string_byte_index_to_char(str, i);
        }
    }

    return -1;
}
bool string_begins_with(const String* str, const String* prefix) {
    if (!str || !prefix) return false;

    size_t prefix_len = string_length(prefix);
    if (prefix_len > str->char_len) return false;

    // Compare the beginning of the string with the prefix
    return memcmp(str->data, prefix->data, prefix_len) == 0;
}

bool string_begins_with_cstr(const String* str, const char* prefix) {
    if (!str || !prefix) return false;

    size_t prefix_len = strlen(prefix);
    if (prefix_len > str->char_len) return false;

    // Compare the beginning of the string with the prefix
    return memcmp(str->data, prefix, prefix_len) == 0;
}

String* string_isplit(const String* str, const char* delim, size_t index, void* tag) {
    if (!str || !delim) return NULL;

    // Split the string into substrings based on the delimiter
    String* result;

    size_t delim_len = strlen(delim);
    size_t start = 0;
    size_t end = 0;
    size_t count = 0;

    while ((end = string_find_cstr(str, delim, start)) != SIZE_MAX) {
        if (count == index) {
            result = string_substring(str, start, end - start, tag);
            return result;
        }
        count++;
        start = end + delim_len;
    }

    // Handle the last substring after the last delimiter
    if (count == index) {
        result = string_substring(str, start, str->byte_len - start, tag);
        return result;
    }

    // If index is out of bounds
    return NULL;
}

String* string_trim(String* str, const char* chars_to_trim, void* tag) {
    if (!str || !chars_to_trim) return str;

    size_t start = 0;
    size_t end = str->byte_len;

    // Trim leading characters
    while (start < end && strchr(chars_to_trim, str->data[start])) {
        start++;
    }

    // Trim trailing characters
    while (end > start && strchr(chars_to_trim, str->data[end - 1])) {
        end--;
    }

    return string_substring(str, start, end - start, tag);
}

// --- File I/O Implementation ---

// Creates a new string by reading the entire content of a file.
String* string_from_file(const char* filepath, void* tag) {
    if (!filepath) return NULL;

    FILE* file = fopen(filepath, "rb"); // Open in binary read mode
    if (!file) {
        perror("Error opening file for reading");
        return NULL;
    }

    // Determine file size
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error seeking in file");
        fclose(file);
        return NULL;
    }
    long file_size_long = ftell(file);
    if (file_size_long < 0) {
        perror("Error getting file size");
        fclose(file);
        return NULL;
    }
    size_t file_size = (size_t)file_size_long;
    rewind(file); // Go back to the beginning

    // Allocate buffer to hold file content
    // Use malloc directly here as this buffer is temporary
    char* buffer = malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Error allocating memory for file content\n");
        fclose(file);
        return NULL;
    }

    // Read file content
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        if (ferror(file)) {
            perror("Error reading file");
        } else {
            fprintf(stderr, "Error reading file: Unexpected end of file\n");
        }
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);

    // Create string from buffer content using the provided tag
    String* result = string_new_len(buffer, file_size, tag);

    // Free the temporary buffer
    free(buffer);

    // string_new_len returns NULL on allocation failure
    return result;
}

// Writes the content of a string to a file.
bool string_to_file(const String* str, const char* filepath) {
    if (!str || !filepath) return false;

    FILE* file = fopen(filepath, "wb"); // Open in binary write mode (truncates existing file)
    if (!file) {
        perror("Error opening file for writing");
        return false;
    }

    // Write string data
    size_t bytes_written = fwrite(str->data, 1, str->byte_len, file);

    if (bytes_written != str->byte_len) {
        perror("Error writing to file");
        fclose(file);
        // Attempt to remove partially written file? Optional.
        // remove(filepath);
        return false;
    }

    // Check for other write errors
    if (ferror(file)) {
        perror("Error during file write operation");
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        perror("Error closing file after writing");
        // Data might be written, but closing failed. Still return false? Yes.
        return false;
    }

    return true;
}
