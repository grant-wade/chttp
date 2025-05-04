#ifndef CSTRING_H
#define CSTRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Represents a UTF-8 encoded string.
 * The string is stored as a dynamically allocated buffer.
 * The `byte_len` field indicates the length of the string in bytes,
 * while `char_len` indicates the number of UTF-8 characters.
 * The `capacity` field indicates the allocated size of the buffer.
 * The `tag` field is used for memory management.
 */
typedef struct {
    char* data;         // UTF-8 encoded string data
    size_t byte_len;    // Length in bytes
    size_t char_len;    // Length in characters (cached)
    size_t capacity;    // Allocated capacity in bytes
    void* tag;          // Tag for memory management
} String;

typedef uint32_t rune;

// Initial capacity for new strings
#define STRING_INITIAL_CAPACITY 16

// Growth factor for resizing strings
#define STRING_GROWTH_FACTOR 2


// String creation functions

/**
 * @brief Creates a new string from a null-terminated C string.
 * @param cstr The null-terminated C string to copy. If NULL, creates an empty string.
 * @param tag A memory allocation tag.
 * @return A pointer to the newly created string, or NULL on allocation failure.
 */
String* string_new(const char* cstr, void* tag);

/**
 * @brief Creates a new string from a character array with a specified length.
 * @param str The character array (does not need to be null-terminated).
 * @param len The number of bytes to copy from the array.
 * @param tag A memory allocation tag.
 * @return A pointer to the newly created string, or NULL on allocation failure.
 */
String* string_new_len(const char* str, size_t len, void* tag);

/**
 * @brief Creates a new, empty string.
 * @param tag A memory allocation tag.
 * @return A pointer to the newly created empty string, or NULL on allocation failure.
 */
String* string_new_empty(void* tag);

/**
 * @brief Creates a new string from an existing allocated char array data, taking ownership of the buffer.
 * @param str The character array (does not need to be null-terminated).
 * @param len The number of bytes to copy from the array.
 * @param tag A memory allocation tag.
 * @return A pointer to the newly created string, or NULL on allocation failure.
 */
String* string_new_from_owned(const char* str, size_t len, void* tag);


/**
 * @brief Creates a copy of an existing string.
 * @param other The string to copy.
 * @param tag A memory allocation tag.
 * @return A pointer to the newly created copy, or NULL if `other` is NULL or on allocation failure.
 */
String* string_copy(const String* other, void* tag);

// String destruction

/**
 * @brief Frees the memory allocated for a string, including its internal buffer.
 * @param str The string to free. If NULL, the function does nothing.
 */
void string_free(String* str);

// String operations

/**
 * @brief Gets the number of UTF-8 characters in the string.
 * @param str The string.
 * @return The number of characters, or 0 if the string is NULL.
 */
size_t string_length(const String* str);

/**
 * @brief Gets the number of bytes in the string's internal buffer (excluding the null terminator).
 * @param str The string.
 * @return The number of bytes, or 0 if the string is NULL.
 */
size_t string_byte_length(const String* str);

/**
 * @brief Gets a pointer to the raw byte data of the string.
 * @warning The returned pointer is NOT guaranteed to be null-terminated if the string was created
 *          with `string_new_len` and contains null bytes internally. Use `string_cstr` for a
 *          guaranteed null-terminated C string representation.
 * @param str The string.
 * @return A pointer to the internal byte buffer, or NULL if the string is NULL.
 */
char* string_bytes(const String* str);

/**
 * @brief Gets a pointer to a null-terminated C string representation of the string.
 * @param str The string.
 * @return A pointer to the internal null-terminated C string, or NULL if the string is NULL.
 */
const char* string_cstr(const String* str);

// Comparison

/**
 * @brief Checks if two strings are equal (byte-wise comparison).
 * @param str1 The first string.
 * @param str2 The second string.
 * @return `true` if the strings are equal or both are NULL, `false` otherwise.
 */
bool string_equals(const String* str1, const String* str2);

/**
 * @brief Checks if a string is equal to a null-terminated C string (byte-wise comparison).
 * @param str The string.
 * @param cstr The null-terminated C string.
 * @return `true` if the string's content is equal to the C string, `false` otherwise or if either is NULL.
 */
bool string_equals_cstr(const String* str, const char* cstr);

/**
 * @brief Compares two strings lexicographically (byte-wise).
 * @param str1 The first string.
 * @param str2 The second string.
 * @return An integer less than, equal to, or greater than zero if `str1` is found,
 *         respectively, to be less than, to match, or be greater than `str2`.
 *         NULL strings are considered less than non-NULL strings.
 */
int string_compare(const String* str1, const String* str2);

// Modification

/**
 * @brief Appends the content of another string to the end of this string.
 * @param str The string to append to (will be modified).
 * @param other The string whose content will be appended.
 */
void string_append(String* str, const String* other);

/**
 * @brief Appends a null-terminated C string to the end of this string.
 * @param str The string to append to (will be modified).
 * @param cstr The null-terminated C string to append.
 */
void string_append_cstr(String* str, const char* cstr);

/**
 * @brief Appends a single Unicode character (code point) to the end of the string.
 * @param str The string to append to (will be modified).
 * @param codepoint The Unicode code point to append.
 */
void string_append_char(String* str, rune codepoint);

/**
 * @brief Appends a byte array to the end of the string. It can contain null bytes.
 * @param str The string to append to (will be modified).
 * @param bytes The byte array to append.
 * @param len The number of bytes to append.
 */
void string_append_bytes(String* str, const uint8_t* bytes, size_t len);

/**
 * @brief Creates a new string containing a substring of the original string.
 * @param str The original string.
 * @param start The starting character index (inclusive).
 * @param length The number of characters to include in the substring.
 * @param tag A memory allocation tag for the new string.
 * @return A new string containing the specified substring, or an empty string if parameters are invalid.
 *         Returns NULL on allocation failure.
 */
String* string_substring(const String* str, size_t start, size_t length, void* tag);

/**
 * @brief Computes a hash value for the string.
 * @param str The string to hash.
 * @return A 64-bit hash value.
 */
uint64_t string_hash(const String* str);

/**
 * @brief Clears the content of the string, making it empty.
 *        The allocated capacity is retained.
 * @param str The string to clear.
 */
void string_clear(String* str);

// Character operations

/**
 * @brief Gets the Unicode code point of the character at the specified index.
 * @param str The string.
 * @param index The character index (0-based).
 * @return The Unicode code point at the index, or 0 if the index is out of bounds or the string is NULL.
 */
rune string_char_at(const String* str, size_t index);

/**
 * @brief Converts a character index to its corresponding byte index within the string's buffer.
 * @param str The string.
 * @param char_index The character index (0-based).
 * @return The byte index corresponding to the start of the character, or the string's byte length
 *         if the character index is out of bounds. Returns 0 if str is NULL.
 */
size_t string_char_index_to_byte(const String* str, size_t char_index);

/**
 * @brief Converts a byte index to its corresponding character index.
 *        If the byte index points within a multi-byte character, it returns the index of that character.
 * @param str The string.
 * @param byte_index The byte index (0-based).
 * @return The character index corresponding to the byte index, or the string's character length
 *         if the byte index is out of bounds. Returns 0 if str is NULL.
 */
size_t string_byte_index_to_char(const String* str, size_t byte_index);

// Search operations

/**
 * @brief Finds the first occurrence of a substring within a string, starting from a given position.
 * @param str The string to search within.
 * @param substr The string to search for.
 * @param start_pos The character index from where to start the search.
 * @return The character index of the first occurrence of `substr` at or after `start_pos`,
 *         or -1 if not found or if parameters are invalid.
 */
size_t string_find(const String* str, const String* substr, size_t start_pos);

/**
 * @brief Finds the first occurrence of a C string within a string, starting from a given position.
 * @param str The string to search within.
 * @param substr The null-terminated C string to search for.
 * @param start_pos The character index from where to start the search.
 * @return The character index of the first occurrence of `substr` at or after `start_pos`,
 *         or -1 if not found or if parameters are invalid.
 */
size_t string_find_cstr(const String* str, const char* substr, size_t start_pos);

/**
 * @brief Checks if the string starts with a given prefix.
 * @param str The string to check.
 * @param prefix The prefix to check for.
 * @return `true` if the string starts with the prefix, `false` otherwise.
 */
bool string_begins_with(const String* str, const String* prefix);

/**
 * @brief Checks if the string starts with a null-terminated C string prefix.
 * @param str The string to check.
 * @param prefix The null-terminated C string prefix to check for.
 * @return `true` if the string starts with the prefix, `false` otherwise.
 */
bool string_begins_with_cstr(const String* str, const char* prefix);


/**
 * @brief Splits a string into substrings based on a delimiter string.
 * @param str The string to split.
 * @param delim The delimiter string (null-terminated).
 * @param index The index of the substring to return (0-based).
 * @param tag A memory allocation tag for the new substring.
 * @return A new string containing the specified substring, or NULL if the index is out of bounds
 *         or if the string is NULL. Returns NULL on allocation failure.
 */
String* string_isplit(const String* str, const char* delim, size_t index, void* tag);


/**
 * @brief Trims leading and trailing characters from a string returning a new string.
 * @param str The string to trim.
 * @param chars_to_trim The characters to trim (null-terminated).
 * @param tag A memory allocation tag for the new string.
 * @return A new string with the trimmed content, or NULL if the string is NULL or on allocation failure.
 */
String* string_trim(String* str, const char* chars_to_trim, void* tag);


// UTF-8 utilities

/**
 * @brief Checks if a given byte sequence represents a valid UTF-8 string.
 * @param str The byte sequence.
 * @param len The length of the byte sequence.
 * @return `true` if the sequence is valid UTF-8, `false` otherwise.
 */
bool string_is_valid_utf8(const char* str, size_t len);

/**
 * @brief Gets the number of bytes occupied by the UTF-8 character starting at the given pointer.
 * @param str Pointer to the start of a UTF-8 character.
 * @return The number of bytes (1-4) for the character, or 1 if it's an invalid starting byte, or 0 if str is NULL or points to a null byte.
 */
size_t string_utf8_char_len(const char* str);

/**
 * @brief Decodes a single UTF-8 character from a byte sequence into a Unicode code point.
 * @param str Pointer to the start of the UTF-8 character sequence.
 * @param bytes_read Optional pointer to a size_t variable that will receive the number of bytes read (1-4).
 * @return The decoded Unicode code point, or 0xFFFD (replacement character) if the sequence is invalid. Returns 0 if str is NULL or points to a null byte.
 */
rune string_decode_utf8_char(const char* str, size_t* bytes_read);

/**
 * @brief Encodes a Unicode code point into its UTF-8 byte sequence.
 * @param codepoint The Unicode code point to encode.
 * @param buffer A buffer large enough to hold the encoded sequence (at least 4 bytes recommended).
 * @return The number of bytes written to the buffer (1-4), or 3 if the codepoint is invalid (writes the replacement character). Returns 0 if buffer is NULL.
 */
size_t string_encode_utf8_char(uint32_t codepoint, char* buffer);

// File I/O

/**
 * @brief Creates a new string by reading the entire content of a file.
 * @param filepath The path to the file to read.
 * @param tag A memory allocation tag for the new string.
 * @return A new string containing the file content, or NULL if the file cannot be read or on allocation failure.
 *         The file content is read as raw bytes.
 */
String* string_from_file(const char* filepath, void* tag);

/**
 * @brief Writes the content of a string to a file, overwriting the file if it exists.
 * @param str The string whose content will be written.
 * @param filepath The path to the file to write to.
 * @return `true` if the write operation was successful, `false` otherwise (e.g., file cannot be opened, write error).
 */
bool string_to_file(const String* str, const char* filepath);

#endif /* CSTRING_H */
