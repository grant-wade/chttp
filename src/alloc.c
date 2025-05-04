#include "alloc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Hash tables
static alloc_info** g_alloc_table = NULL;
static size_t g_alloc_table_size = 0;
static size_t g_alloc_count = 0;

static tag_entry** g_tag_table = NULL;
static size_t g_tag_table_size = 0;
static size_t g_tag_count = 0;

static int g_allocator_initialized = 0;

// Simple hash function for pointers
static size_t hash_pointer(const void* ptr, size_t table_size) {
    return ((uintptr_t)ptr >> 3) % table_size;
}

// Initialize the allocator tables
static void allocator_init_once(void) {
    if (!g_allocator_initialized) {
        // Create allocation table
        g_alloc_table_size = ALLOC_TABLE_INITIAL_SIZE;
        g_alloc_table = calloc(g_alloc_table_size, sizeof(alloc_info*));

        // Create tag table
        g_tag_table_size = TAG_TABLE_INITIAL_SIZE;
        g_tag_table = calloc(g_tag_table_size, sizeof(tag_entry*));

        g_allocator_initialized = 1;
    }
}

// Find allocation info for a pointer
alloc_info* find_alloc_info(void* ptr) {
    if (!ptr || !g_alloc_table) return NULL;

    size_t index = hash_pointer(ptr, g_alloc_table_size);
    alloc_info* info = g_alloc_table[index];

    while (info) {
        if (info->ptr == ptr) {
            return info;
        }
        info = info->next;
    }

    return NULL;
}

// Find tag entry for a tag
static tag_entry* find_tag_entry(void* tag) {
    if (!g_tag_table) return NULL;

    size_t index = hash_pointer(tag, g_tag_table_size);
    tag_entry* entry = g_tag_table[index];

    while (entry) {
        if (entry->tag == tag) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

// Add pointer to tag entry
static void add_ptr_to_tag(tag_entry* entry, void* ptr) {
    // Ensure capacity
    if (entry->count >= entry->capacity) {
        size_t new_capacity = entry->capacity * 2;
        if (new_capacity == 0) new_capacity = PTR_LIST_INITIAL_SIZE;

        void** new_ptrs = realloc(entry->ptrs, new_capacity * sizeof(void*));
        if (!new_ptrs) return; // Memory allocation failed

        entry->ptrs = new_ptrs;
        entry->capacity = new_capacity;
    }

    // Add the pointer
    entry->ptrs[entry->count++] = ptr;
}

// Remove pointer from tag entry
static void remove_ptr_from_tag(tag_entry* entry, void* ptr) {
    for (size_t i = 0; i < entry->count; i++) {
        if (entry->ptrs[i] == ptr) {
            // Move the last element to this position (if not already the last)
            if (i < entry->count - 1) {
                entry->ptrs[i] = entry->ptrs[entry->count - 1];
            }
            entry->count--;
            return;
        }
    }
}

// Create new tag entry
static tag_entry* create_tag_entry(void* tag) {
    tag_entry* entry = malloc(sizeof(tag_entry));
    if (!entry) return NULL;

    entry->tag = tag;
    entry->count = 0;
    entry->capacity = PTR_LIST_INITIAL_SIZE;
    entry->ptrs = malloc(entry->capacity * sizeof(void*));
    entry->next = NULL;

    if (!entry->ptrs) {
        free(entry);
        return NULL;
    }

    // Add to tag table
    size_t index = hash_pointer(tag, g_tag_table_size);
    entry->next = g_tag_table[index];
    g_tag_table[index] = entry;
    g_tag_count++;

    return entry;
}

// Register allocation in our tables
static void register_allocation(void* ptr, size_t size, void* tag) {
    alloc_info* info = malloc(sizeof(alloc_info));
    if (!info) return;

    info->ptr = ptr;
    info->size = size;
    info->tag = tag;

    // Add to allocation table
    size_t index = hash_pointer(ptr, g_alloc_table_size);
    info->next = g_alloc_table[index];
    g_alloc_table[index] = info;
    g_alloc_count++;

    // Find or create tag entry and add pointer
    tag_entry* tag_e = find_tag_entry(tag);
    if (!tag_e) {
        tag_e = create_tag_entry(tag);
    }

    if (tag_e) {
        add_ptr_to_tag(tag_e, ptr);
    }
}

// Unregister allocation
static alloc_info* unregister_allocation(void* ptr) {
    if (!ptr) return NULL;

    size_t index = hash_pointer(ptr, g_alloc_table_size);
    alloc_info* info = g_alloc_table[index];
    alloc_info* prev = NULL;

    while (info) {
        if (info->ptr == ptr) {
            // Remove from allocation table
            if (prev) {
                prev->next = info->next;
            }
            else {
                g_alloc_table[index] = info->next;
            }

            g_alloc_count--;

            // Remove from tag entry
            tag_entry* tag_e = find_tag_entry(info->tag);
            if (tag_e) {
                remove_ptr_from_tag(tag_e, ptr);

                // If tag entry is now empty, we could remove it
                if (tag_e->count == 0) {
                    // But that's more complex to do safely here, we'll leave it
                }
            }

            return info;
        }

        prev = info;
        info = info->next;
    }

    return NULL;
}

// Public API implementations

void* pmalloc(size_t size, void* tag) {
    allocator_init_once();
    void* ptr = malloc(size);
    if (!ptr) return NULL;

    // zero-initialize memory
    memset(ptr, 0, size);

    register_allocation(ptr, size, tag);
    return ptr;
}

void* pcalloc(size_t n, size_t size, void* tag) {
    allocator_init_once();
    void* ptr = calloc(n, size);
    if (!ptr) return NULL;

    register_allocation(ptr, n * size, tag);
    return ptr;
}

void* prealloc(void* ptr, size_t size, void* tag) {
    allocator_init_once();
    if (!ptr) return pmalloc(size, tag);

    alloc_info* info = find_alloc_info(ptr);
    if (!info) {
        assert(0 && "prealloc: pointer not found");
        return NULL;
    }

    // Save necessary data before potential memory move
    void* old_tag = info->tag;
    void* original_ptr = ptr; // Save original pointer BEFORE realloc
    size_t old_hash_index = hash_pointer(original_ptr, g_alloc_table_size);

    // Perform reallocation
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) return NULL;

    if (new_ptr != original_ptr) {
        // Handle pointer change - original ptr is now invalid

        // Find and remove from allocation table using saved hash index
        alloc_info* curr = g_alloc_table[old_hash_index];
        alloc_info* prev = NULL;

        while (curr) {
            if (curr->ptr == original_ptr) {
                if (prev) {
                    prev->next = curr->next;
                }
                else {
                    g_alloc_table[old_hash_index] = curr->next;
                }
                g_alloc_count--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }

        // Remove original ptr from tag entry
        tag_entry* old_tag_e = find_tag_entry(old_tag);
        if (old_tag_e) {
            // We need to search and remove specifically
            for (size_t i = 0; i < old_tag_e->count; i++) {
                if (old_tag_e->ptrs[i] == original_ptr) {
                    // Move the last element to this position
                    if (i < old_tag_e->count - 1) {
                        old_tag_e->ptrs[i] = old_tag_e->ptrs[old_tag_e->count - 1];
                    }
                    old_tag_e->count--;
                    break;
                }
            }
        }

        // Free allocation info for old pointer
        free(info);

        // Register the new pointer
        register_allocation(new_ptr, size, tag);
    }
    else {
        // Same pointer was returned, just update in place
        if (tag != old_tag) {
            // Tag changed, update tag entries
            tag_entry* old_tag_e = find_tag_entry(old_tag);
            tag_entry* new_tag_e = find_tag_entry(tag);

            if (old_tag_e) {
                // We can use remove_ptr_from_tag here as the pointer is still valid
                remove_ptr_from_tag(old_tag_e, new_ptr);
            }

            if (!new_tag_e) {
                new_tag_e = create_tag_entry(tag);
            }

            if (new_tag_e) {
                add_ptr_to_tag(new_tag_e, new_ptr);
            }
        }

        info->size = size;
        info->tag = tag;
    }

    return new_ptr;
}

void pfree(void* ptr) {
    if (!ptr) return;

    allocator_init_once();
    alloc_info* info = unregister_allocation(ptr);

    if (!info) {
        assert(0 && "pfree: unknown pointer");
        return;
    }

    free(info);
    free(ptr);
}

void pfree_tag(void* tag) {
    allocator_init_once();

    tag_entry* tag_e = find_tag_entry(tag);
    if (!tag_e || tag_e->count == 0) {
        return; // Nothing to free
    }

    // Copy pointers to a temp array to avoid problems while freeing
    size_t count = tag_e->count;
    void** ptrs = malloc(count * sizeof(void*));
    if (!ptrs) return;

    memcpy(ptrs, tag_e->ptrs, count * sizeof(void*));

    // Free all pointers with this tag
    for (size_t i = 0; i < count; i++) {
        alloc_info* info = find_alloc_info(ptrs[i]);
        if (info) {
            // Use unregister to keep our structures consistent
            unregister_allocation(ptrs[i]);
            free(info);
            free(ptrs[i]);
        }
    }

    free(ptrs);
}

void pallocator_cleanup(void) {
    if (!g_allocator_initialized) return;

    // Free all allocated memory
    for (size_t i = 0; i < g_alloc_table_size; i++) {
        alloc_info* info = g_alloc_table[i];
        while (info) {
            alloc_info* next = info->next;
            free(info->ptr); // Free the allocated memory
            free(info); // Free the tracking structure
            info = next;
        }
    }

    // Free tag entries
    for (size_t i = 0; i < g_tag_table_size; i++) {
        tag_entry* entry = g_tag_table[i];
        while (entry) {
            tag_entry* next = entry->next;
            free(entry->ptrs);
            free(entry);
            entry = next;
        }
    }

    // Free tables
    free(g_alloc_table);
    free(g_tag_table);

    // Reset state
    g_alloc_table = NULL;
    g_tag_table = NULL;
    g_alloc_table_size = 0;
    g_tag_table_size = 0;
    g_alloc_count = 0;
    g_tag_count = 0;
    g_allocator_initialized = 0;
}

// Pretty print the current state of memory allocations
void palloc_print_state(void) {
    if (!g_allocator_initialized) {
        printf("Memory allocator not initialized.\n");
        return;
    }

    printf("\n=== Memory Allocator State ===\n");
    printf("Total allocations: %zu\n", g_alloc_count);
    printf("Total tags: %zu\n", g_tag_count);

    // Calculate total bytes
    size_t total_bytes = 0;
    for (size_t i = 0; i < g_alloc_table_size; i++) {
        alloc_info* info = g_alloc_table[i];
        while (info) {
            total_bytes += info->size;
            info = info->next;
        }
    }

    printf("Total memory: %zu bytes (%.2f KB, %.2f MB)\n",
           total_bytes,
           (double)total_bytes / 1024.0,
           (double)total_bytes / (1024.0 * 1024.0));

    printf("\n--- Memory by Tag ---\n");

    // Go through each tag and print its allocations
    for (size_t i = 0; i < g_tag_table_size; i++) {
        tag_entry* entry = g_tag_table[i];

        while (entry) {
            size_t tag_bytes = 0;

            // Calculate total bytes for this tag
            for (size_t j = 0; j < entry->count; j++) {
                alloc_info* info = find_alloc_info(entry->ptrs[j]);
                if (info) {
                    tag_bytes += info->size;
                }
            }

            printf("\nTag %p: %zu allocations, %zu bytes\n",
                   entry->tag, entry->count, tag_bytes);

            // Print details for each allocation with this tag
            for (size_t j = 0; j < entry->count; j++) {
                alloc_info* info = find_alloc_info(entry->ptrs[j]);
                if (info) {
                    printf("  [%zu] Ptr: %p, Size: %zu bytes\n",
                           j, info->ptr, info->size);

                    // For small allocations, try to show content (up to 16 bytes)
                    if (info->size <= 512 && info->size > 0) {
                        unsigned char* data = (unsigned char*)info->ptr;
                        int printable = 1;

                        // Check if data appears to be printable text
                        for (size_t k = 0; k < 16 && k < info->size; k++) {
                            if (data[k] != 0 && (data[k] < 32 || data[k] > 126)) {
                                printable = 0;
                                break;
                            }
                        }

                        if (printable) {
                            // Print as string
                            printf("      Content: \"");
                            for (size_t k = 0; k < 32 && k < info->size && data[k] != 0; k++) {
                                printf("%c", data[k]);
                            }
                            printf("\"\n");
                        }
                        else {
                            // Print as hex dump
                            printf("      Hex dump: ");
                            for (size_t k = 0; k < 16 && k < info->size; k++) {
                                printf("%02x ", data[k]);
                                if (k == 7) printf(" "); // Visual separator after 8 bytes
                            }
                            printf("\n");
                        }
                    }
                }
            }

            entry = entry->next;
        }
    }

    printf("\n=== End Memory Allocator State ===\n\n");
}

// Inspect a specific memory location with detailed hex and ASCII display
void pinspect(void* ptr) {
    if (!ptr) {
        printf("Cannot inspect null pointer\n");
        return;
    }

    allocator_init_once();

    printf("\n=== Memory Inspection for %p ===\n", ptr);

    // Try to find allocation info
    alloc_info* info = find_alloc_info(ptr);
    if (info) {
        printf("TRACKED MEMORY: %zu bytes, Tag: %p\n", info->size, info->tag);

        // Find which tag entry contains this pointer
        tag_entry* tag_e = find_tag_entry(info->tag);
        if (tag_e) {
            // Find index of this pointer in tag's pointer list
            size_t index = 0;
            for (size_t i = 0; i < tag_e->count; i++) {
                if (tag_e->ptrs[i] == ptr) {
                    index = i;
                    break;
                }
            }
            printf("Allocation #%zu of %zu with this tag\n", index + 1, tag_e->count);
        }
        else {
            printf("Warning: Tag entry not found, data structures may be inconsistent\n");
        }
    }
    else {
        printf("UNTRACKED MEMORY: Cannot determine size safely\n");
        printf("Warning: Using default inspection length of 128 bytes\n");
    }

    // Size to display (use actual size for tracked memory, default for untracked)
    size_t display_size = info ? info->size : 128;
    unsigned char* data = (unsigned char*)ptr;

    printf("\nOffset  Hexadecimal                                       ASCII\n");
    printf("--------  ------------------------------------------------  ------------------\n");

    // Show memory in chunks of 16 bytes
    for (size_t offset = 0; offset < display_size; offset += 16) {
        // Print offset
        printf("%08zx  ", offset);

        // Print hex values
        for (size_t i = 0; i < 16; i++) {
            if (offset + i < display_size) {
                printf("%02x ", data[offset + i]);
            }
            else {
                printf("   "); // Padding for incomplete line
            }

            if (i == 7) {
                printf(" "); // Extra space after 8 bytes
            }
        }

        // Print ASCII representation
        printf(" |");
        int char_count = 0;
        for (size_t i = 0; i < 16 && (offset + i < display_size); i++) {
            unsigned char c = data[offset + i];
            // Print only printable characters, replace others with dots
            if (c >= 32 && c <= 126) {
                printf("%c", c);
            }
            else {
                printf(".");
            }
            char_count++;
        }
        while (char_count < 16) {
            printf(" "); // Padding for incomplete line
            char_count++;
        }
        printf("|\n");

        // For large allocations, show only the first and last portions
        if (display_size > 128 && offset == 48) {
            printf("...      ... similar content omitted ...\n");
            offset = display_size - 64; // Skip to last 64 bytes
        }
    }

    printf("--------  ------------------------------------------------  ------------------\n");
    printf("=== End Memory Inspection ===\n\n");
}


size_t ptag_size(void* tag) {
    if (!tag) return 0;

    allocator_init_once();
    tag_entry* entry = find_tag_entry(tag);
    if (!entry) return 0;

    size_t total_size = 0;
    for (size_t i = 0; i < entry->count; i++) {
        alloc_info* info = find_alloc_info(entry->ptrs[i]);
        if (info) {
            total_size += info->size;
        }
    }

    return total_size;
}
