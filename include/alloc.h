//
// Created by Grant Wade on 4/27/25.
//
#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

// Data structures for allocator
typedef struct alloc_info {
    void* ptr; // Allocated pointer
    size_t size; // Size of allocation
    void* tag; // Tag associated with allocation
    struct alloc_info* next; // For hash collision chaining
} alloc_info;

typedef struct tag_entry {
    void* tag; // Tag value
    void** ptrs; // Array of pointers with this tag
    size_t count; // Number of pointers
    size_t capacity; // Current capacity of the ptrs array
    struct tag_entry* next; // For hash collision chaining
} tag_entry;

// Constants for hash tables
#define ALLOC_TABLE_INITIAL_SIZE 256
#define TAG_TABLE_INITIAL_SIZE 32
#define LOAD_FACTOR_THRESHOLD 0.75
#define PTR_LIST_INITIAL_SIZE 8

#define TAG(value) (void*)(size_t)value

/**
 * @brief Allocates memory and tracks it with a tag.
 *
 * This function allocates `size` bytes of memory, similar to `malloc`,
 * but also associates the allocation with a `tag`. This allows for
 * grouped freeing and tracking.
 *
 * @param size The number of bytes to allocate.
 * @param tag A pointer used as a tag to categorize this allocation.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void* pmalloc(size_t size, void* tag);

/**
 * @brief Allocates memory for an array, initializes it to zero, and tracks it with a tag.
 *
 * This function allocates memory for an array of `nmemb` elements, each of `size` bytes,
 * similar to `calloc`. The allocated memory is initialized to zero.
 * The allocation is associated with the provided `tag`.
 *
 * @param nmemb The number of elements to allocate.
 * @param size The size of each element in bytes.
 * @param tag A pointer used as a tag to categorize this allocation.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void* pcalloc(size_t nmemb, size_t size, void* tag);

/**
 * @brief Reallocates a previously allocated memory block, potentially changing its tag.
 *
 * This function changes the size of the memory block pointed to by `ptr` to `size` bytes,
 * similar to `realloc`. The contents will be unchanged up to the minimum of the old
 * and new sizes. If `ptr` is NULL, it behaves like `pmalloc(size, tag)`.
 * If the reallocation is successful, the allocation is associated with the new `tag`.
 *
 * @param ptr A pointer to the memory block previously allocated by `pmalloc`, `pcalloc`, or `prealloc`.
 *            If NULL, a new block is allocated.
 * @param size The new size for the memory block in bytes.
 * @param tag A pointer used as the new tag for this allocation.
 * @return A pointer to the reallocated memory block, or NULL if reallocation fails.
 *         The returned pointer may be different from `ptr`.
 */
void* prealloc(void* ptr, size_t size, void* tag);

/**
 * @brief Frees a previously allocated memory block.
 *
 * This function frees the memory block pointed to by `ptr`, which must have been
 * returned by a previous call to `pmalloc`, `pcalloc`, or `prealloc`.
 * If `ptr` is NULL, no operation is performed.
 *
 * @param ptr A pointer to the memory block to free.
 */
void pfree(void* ptr);

/**
 * @brief Frees all memory blocks associated with a specific tag.
 *
 * This function finds all memory allocations associated with the given `tag`
 * and frees them.
 *
 * @param tag The tag whose associated memory blocks should be freed.
 */
void pfree_tag(void* tag);

/**
 * @brief Finds the allocation information for a given pointer.
 *
 * This function searches for the allocation information associated with the
 * provided pointer. It returns a pointer to the `alloc_info` structure if found,
 * or NULL if not found.
 *
 * @param ptr The pointer to search for.
 * @return A pointer to the `alloc_info` structure, or NULL if not found.
 */
alloc_info* find_alloc_info(void* ptr);

/**
 * @brief Cleans up the allocator, freeing all tracked allocations.
 *
 * This function frees all memory blocks currently tracked by the allocator,
 * regardless of their tag. It also releases internal resources used by the allocator.
 * After calling this, the allocator is reset to an uninitialized state.
 */
void pallocator_cleanup(); // free all outstanding allocations

/**
 * @brief Prints the current state of tracked memory allocations.
 *
 * This function outputs a summary of the memory currently managed by the
 * allocator, including total allocations, total memory usage, and a breakdown
 * by tag with details for each allocation. Useful for debugging memory usage.
 */
void palloc_print_state(void);

/**
 * @brief Inspects the content of a memory location with hex and ASCII display.
 *
 * This function displays the content of the memory pointed to by `ptr`.
 * If the pointer corresponds to a tracked allocation, it shows the allocation
 * details (size, tag) and displays the entire allocated block (or a summary
 * for large blocks). If the pointer is not tracked, it displays a default
 * number of bytes (e.g., 128) starting from `ptr`.
 *
 * @param ptr The memory address to inspect.
 */
void pinspect(void* ptr);

/**
 * @brief Gets the size of all memory used by a specific tag.
 *
 * @param tag The tag whose associated memory block size to retrieve.
 * @return The size of the memory block associated with the tag, or 0 if not found.
 */
size_t ptag_size(void* tag);


#endif // ALLOC_H
