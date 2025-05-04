/*
------------------------------------------------------------------------------
cbuild.h - Minimal C build system as a C header library

Why cbuild.h?
-------------
cbuild.h lets you describe your C build in C code, not in Makefiles or shell scripts.
- Write your build logic in C, with full language power (loops, conditionals, variables).
- Cross-platform: works on Windows, macOS, Linux.
- No dependencies: just a single header, no Python, no Lua, no external tools.
- Fast incremental builds: only rebuilds what changed.
- Supports parallel compilation, (dependency tracking, probably broken), custom commands, and subcommands.

Typical Use Case:
-----------------
- You want a simple, portable build for a C project.
- You want to avoid the complexity of Make, CMake, or scripting languages.
- You want to keep your build logic versioned and hackable in C.

How to Use:
-----------
1. Write a `build.c` file that includes and implements cbuild.h:

    #define CBUILD_IMPLEMENTATION
    #include "cbuild.h"

2. Describe your build targets in C:

    target_t* mylib = cbuild_static_library("mylib");
    cbuild_add_source(mylib, "src/foo.c");
    cbuild_add_include_dir(mylib, "include");

    target_t* app = cbuild_executable("myapp");
    cbuild_add_source(app, "src/main.c");
    cbuild_target_link_library(app, mylib);

3. Optionally, add custom commands or subcommands:

    cbuild_register_subcommand("test", app, "./build/myapp --run-tests", NULL, NULL);

4. In your main(), call cbuild_run(argc, argv):

    int main(int argc, char** argv) {
        cbuild_set_output_dir("build");
        // ... define targets ...
        return cbuild_run(argc, argv);
    }

5. Build your project:

    $ gcc build.c -o cbuild
    $ ./cbuild           # builds all targets
    $ ./cbuild clean     # cleans build outputs
    $ ./cbuild myapp     # builds only 'myapp' and its dependencies
    $ ./cbuild test      # builds 'app' and runs the test subcommand

Self-Rebuilding:
----------------
If you want your build executable to auto-rebuild itself when build.c or cbuild.h changes,
call cbuild_self_rebuild_if_needed() at the start of main():

    const char* self_sources[] = {"build.c", "cbuild.h"};
    cbuild_self_rebuild_if_needed(argc, argv, self_sources, 2);

Example:
--------
    #define CBUILD_IMPLEMENTATION
    #include "cbuild.h"

    int main(int argc, char** argv) {
        cbuild_set_output_dir("build");
        target_t* lib = cbuild_static_library("foo");
        cbuild_add_source(lib, "foo.c");
        target_t* exe = cbuild_executable("bar");
        cbuild_add_source(exe, "bar.c");
        cbuild_target_link_library(exe, lib);
        return cbuild_run(argc, argv);
    }

Inspirations:
- nob.h - https://github.com/tsoding/nob.h
- tup   - https://github.com/gittup/tup
- make? lol

------------------------------------------------------------------------------
*/

#ifndef CBUILD_H
#define CBUILD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle for build targets (forward declaration) */
typedef struct cbuild_target target_t;

/* Opaque handle for build commands (forward declaration) */
typedef struct cbuild_command command_t;

/* Subcommand callback definition */
typedef void (*cbuild_subcommand_callback)(void *user_data);

/* --- Public API: Target Creation --- */

/** Create a new executable target with the given name (no file extension needed). */
target_t* cbuild_executable(const char *name);

/** Create a new static library target with the given name. (On Unix, 'lib' prefix and .a will be added) */
target_t* cbuild_static_library(const char *name);

/** Create a new shared library target with the given name. (Adds .dll, .so, or .dylib extension as appropriate) */
target_t* cbuild_shared_library(const char *name);

/* --- Public API: Command Creation and Execution --- */

/** Create a shell command to be run as part of the build graph.
    The command string is executed as-is by the shell. */
command_t* cbuild_command(const char *name, const char *command_line);

/** Add a command as a dependency of a target (runs before the target is built). */
void cbuild_target_add_command(target_t *target, command_t *cmd);

/** Add a command as a dependency of another command (runs before the command). */
void cbuild_command_add_dependency(command_t *cmd, command_t *dependency);

/** Run a command immediately (not as part of the build graph). Returns 0 on success. */
int cbuild_run_command(command_t *cmd);

/** Add a command to be run after the target is built (post-build step). */
void cbuild_target_add_post_command(target_t *target, command_t *cmd);

/* --- Public API: Modifying Targets --- */

/** Add a source file to a target. The source file can be C (or C++) source code. */
void cbuild_add_source(target_t *target, const char *source_file);

/** Add an include directory for a target (passed to compiler as -I or /I). */
void cbuild_add_include_dir(target_t *target, const char *include_path);

/** Add a library search directory for a target's link phase (passed to linker as -L or /LIBPATH). */
void cbuild_add_library_dir(target_t *target, const char *lib_dir);

/** Link an external library to a target by name.
    For GCC/Clang, use names like "m" for math (adds -lm).
    For MSVC, use library base name (e.g., "User32" for User32.lib). */
void cbuild_add_link_library(target_t *target, const char *lib_name);

/** Declare that target `dependant` links against target `dependency`.
    This means `dependency` will be built first, and the output library will be linked into `dependant`. */
void cbuild_target_link_library(target_t *dependant, target_t *dependency);

/** Add a command as a dependency of a target (runs before the target is built). */
void cbuild_target_add_command(target_t *target, command_t *cmd);

/* --- Public API: Global Build Settings --- */

/** Set a custom output directory for all build artifacts (object files, libs, executables).
    Default is "build". */
void cbuild_set_output_dir(const char *dir);

/** Set the number of parallel compile jobs. Default is number of CPU cores (at least 1). */
void cbuild_set_parallelism(int jobs_count);

/** Manually specify the C compiler to use (e.g., "gcc", "clang", "cl").
    If not set, cbuild auto-detects or uses environment variable CC. */
void cbuild_set_compiler(const char *compiler_exe);

/** Specify additional global compiler flags (applied to all targets). Optional. */
void cbuild_add_global_cflags(const char *flags);

/** Specify additional global linker flags for executables/shared libs. Optional. */
void cbuild_add_global_ldflags(const char *flags);

/** Enable dependency tracking (header dependency detection and .d file generation). Disabled by default. */
void cbuild_enable_dep_tracking(int enabled);

/**
 * Checks if the running executable is out-of-date with respect to the given sources.
 * If so, moves itself to .old, rebuilds, and execs the new binary with the same arguments.
 * Call this at the start of main().
 *
 * @param argc main's argc
 * @param argv main's argv
 * @param sources array of source file paths (e.g. {"build.c", "cbuild.h"})
 * @param sources_count number of source files
 */
void cbuild_self_rebuild_if_needed(int argc, char **argv, const char **sources, int sources_count);

/** Enable or disable generation of compile_commands.json */
void cbuild_enable_compile_commands(int enabled);

/* --- Public API: Build Execution --- */

/** Execute the build. Call this in main() with the program arguments.
    Recognized commands:
      - no arguments: build all targets
      - "clean": remove built files
      - target name(s): build only those targets (and their dependencies)
    Returns 0 on success, nonzero on failure. */
int cbuild_run(int argc, char **argv);

/* --- Public API: Custom Subcommands --- */

/**
 * Register a custom subcommand.
 * @param name The subcommand name (e.g. "test")
 * @param target The target that must be built before the subcommand runs
 * @param command_line Shell command to run (optional, can be NULL if using callback)
 * @param callback User callback function to run (optional, can be NULL if using command_line)
 * @param user_data User data pointer passed to callback (can be NULL)
 */
void cbuild_register_subcommand(const char *name, target_t *target, const char *command_line, cbuild_subcommand_callback callback, void *user_data);

/* --- Public API Helper Macros for defining builds --- */

// Self‑rebuild if build.c or cbuild.h changes
#define CBUILD_SELF_REBUILD(...)                                   \
  do {                                                             \
    const char* _srcs[] = { __VA_ARGS__ };                         \
    cbuild_self_rebuild_if_needed(argc, argv,                      \
      _srcs, (int)(sizeof(_srcs)/sizeof(*_srcs)));                 \
  } while(0)

// Add multiple sources to a target
#define CBUILD_SOURCES(TGT, ...)                                   \
  do {                                                             \
    const char* _a[] = { __VA_ARGS__ };                            \
    for (int _i = 0; _i < (int)(sizeof(_a)/sizeof(*_a)); _i++)     \
      cbuild_add_source(TGT, _a[_i]);                              \
  } while(0)

// Add multiple include directories
#define CBUILD_INCLUDES(TGT, ...)                                  \
  do {                                                             \
    const char* _a[] = { __VA_ARGS__ };                            \
    for (int _i = 0; _i < (int)(sizeof(_a)/sizeof(*_a)); _i++)     \
      cbuild_add_include_dir(TGT, _a[_i]);                         \
  } while(0)

// Add multiple library directories
#define CBUILD_LIB_DIRS(TGT, ...)                                  \
  do {                                                             \
    const char* _a[] = { __VA_ARGS__ };                            \
    for (int _i = 0; _i < (int)(sizeof(_a)/sizeof(*_a)); _i++)     \
      cbuild_add_library_dir(TGT, _a[_i]);                         \
  } while(0)

// Link against multiple libraries
#define CBUILD_LINK_LIBS(TGT, ...)                                 \
  do {                                                             \
    const char* _a[] = { __VA_ARGS__ };                            \
    for (int _i = 0; _i < (int)(sizeof(_a)/sizeof(*_a)); _i++)     \
      cbuild_add_link_library(TGT, _a[_i]);                        \
  } while(0)

// Define an executable target with sources, includes, lib dirs, and libs
#define CBUILD_EXECUTABLE(NAME, ...)                               \
  do {                                                             \
    NAME = cbuild_executable(#NAME);                     \
    __VA_ARGS__                                                   \
  } while(0)

// Define a static library target with sources
#define CBUILD_STATIC_LIBRARY(NAME, ...)                           \
  do {                                                             \
    NAME = cbuild_static_library(#NAME);                 \
    __VA_ARGS__                                                   \
  } while(0)

// Define a shared library target with sources
#define CBUILD_SHARED_LIBRARY(NAME, ...)                           \
  do {                                                             \
    NAME = cbuild_shared_library(#NAME);                 \
    __VA_ARGS__                                                   \
  } while(0)


#ifdef __cplusplus
}
#endif

/* ---------------------------------------------------------------------- */
/* Implementation below (define CBUILD_IMPLEMENTATION in one source file) */
/* ---------------------------------------------------------------------- */
#endif /* CBUILD_H */

#ifdef CBUILD_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <limits.h>  // for PATH_MAX
#ifdef _WIN32
  #include <windows.h>   // Windows API for threads, etc.
  #include <process.h>   // _beginthreadex, _spawn
  #include <direct.h>    // _mkdir
  #include <io.h>        // _access or _stat
#else
  #include <pthread.h>
  #include <unistd.h>
  #include <dirent.h>
  #include <fcntl.h>
#endif

// --- Pretty-printing helpers (ANSI colors) ---
#ifndef _WIN32
#define CBUILD_COLOR_RESET   "\033[0m"
#define CBUILD_COLOR_BOLD    "\033[1m"
#define CBUILD_COLOR_GREEN   "\033[32m"
#define CBUILD_COLOR_YELLOW  "\033[33m"
#define CBUILD_COLOR_BLUE    "\033[34m"
#define CBUILD_COLOR_MAGENTA "\033[35m"
#define CBUILD_COLOR_RED     "\033[31m"
#else
#define CBUILD_COLOR_RESET   ""
#define CBUILD_COLOR_BOLD    ""
#define CBUILD_COLOR_GREEN   ""
#define CBUILD_COLOR_YELLOW  ""
#define CBUILD_COLOR_BLUE    ""
#define CBUILD_COLOR_MAGENTA ""
#define CBUILD_COLOR_RED     ""
#endif

static void cbuild_pretty_step(const char *label, const char *color, const char *fmt, ...) {
    printf("%s%-10s%s ", color, label, CBUILD_COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

static void cbuild_pretty_status(int ok, const char *fmt, ...) {
    if (ok)
        printf("%s%s%s ", CBUILD_COLOR_GREEN, "✔", CBUILD_COLOR_RESET);
    else
        printf("%s%s%s ", CBUILD_COLOR_RED, "✖", CBUILD_COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

#define CBUILD_EXE_SUFFIX ".exe"

static int cbuild__needs_rebuild(const char *exe_path, const char **sources, int sources_count) {
    struct stat st_exe;
    if (stat(exe_path, &st_exe) != 0) return 1;
    for (int i = 0; i < sources_count; ++i) {
        struct stat st_src;
        if (stat(sources[i], &st_src) != 0) continue;
        if (st_src.st_mtime > st_exe.st_mtime) return 1;
    }
    return 0;
}

static void cbuild__exec_new_build(const char *exe_path, int argc, char **argv) {
#ifdef _WIN32
    _spawnv(_P_OVERLAY, exe_path, argv);
    exit(1);
#else
    execv(exe_path, argv);
    perror("execv");
    exit(1);
#endif
}

void cbuild_self_rebuild_if_needed(int argc, char **argv, const char **sources, int sources_count) {
    char exe_path[512];
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len > 0) exe_path[len] = 0;
    else strncpy(exe_path, argv[0], sizeof(exe_path));
#endif

    // Always remove any lingering .old file
    char old_path[520];
    snprintf(old_path, sizeof(old_path), "%s.old", exe_path);
    remove(old_path);

    if (cbuild__needs_rebuild(exe_path, sources, sources_count)) {
        printf("cbuild: Detected changes, rebuilding build executable...\n");
        fflush(stdout);
        rename(exe_path, old_path);

#ifdef _WIN32
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cl /nologo /Fe:%s build.c /I. /Iinclude", exe_path);
#else
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cc -o '%s' build.c -I. -Iinclude", exe_path);
#endif
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "cbuild: Self-rebuild failed!\n");
            exit(1);
        }
        cbuild__exec_new_build(exe_path, argc, argv);
    }
}

/* --- Data Structures for Build Targets, Commands, and Build Config --- */

typedef enum { TARGET_EXECUTABLE, TARGET_STATIC_LIB, TARGET_SHARED_LIB, TARGET_COMMAND } cbuild_target_type;

struct cbuild_command {
    char *name;
    char *command_line;
    command_t **dependencies;
    int dep_count, dep_cap;
    int executed; // internal: has this command been executed?
    int result;   // internal: result code after execution
};

/* Structure representing a build target (executable or library) */
struct cbuild_target {
    cbuild_target_type type;
    char *name;                  // base name of target
    char **sources;              // array of source file paths
    int sources_count;
    int sources_cap;
    char **include_dirs;         // array of include directory paths
    int include_count;
    int include_cap;
    char **lib_dirs;             // library directories for linking
    int lib_dir_count;
    int lib_dir_cap;
    char **link_libs;            // external libraries to link (names or paths)
    int link_lib_count;
    int link_lib_cap;
    target_t **dependencies;     // other targets this target depends on (to link against)
    int dep_count;
    int dep_cap;
    char *cflags;                // extra compile flags specific to this target
    char *ldflags;               // extra linker flags specific to this target
    char *output_file;           // path to final output (exe, .a, .dll/.so)
    char *obj_dir;               // directory for this target's object files (and .d files)
    command_t **commands;        // commands to run before building this target
    int cmd_count, cmd_cap;
    command_t **post_commands;   // commands to run after building this target
    int post_cmd_count, post_cmd_cap;
};

/* Global list of targets */
static target_t **g_targets = NULL;
static int g_target_count = 0;
static int g_target_cap = 0;

/* Global list of commands */
static command_t **g_commands = NULL;
static int g_command_count = 0, g_command_cap = 0;

/* Global build settings */
static char *g_output_dir = NULL;       // base output directory for build files (default "build")
static int   g_parallel_jobs = 0;       // number of parallel compile jobs (0 means not set yet)
static char *g_cc = NULL;              // C compiler command (gcc, clang, cl, etc.)
static char *g_ar = NULL;              // static library archiver command (ar or lib)
static char *g_ld = NULL;              // linker command if needed (usually same as compiler for exec/shared)
static char *g_global_cflags = NULL;   // global compiler flags (like debug symbols, optimizations)
static char *g_global_ldflags = NULL;  // global linker flags
static int   g_dep_tracking = 0;       // dependency tracking disabled by default

/* Global list of subcommands */
typedef struct cbuild_subcommand {
    char *name;
    target_t *target;
    char *command_line;
    cbuild_subcommand_callback callback;
    void *user_data;
} cbuild_subcommand_t;

static cbuild_subcommand_t **g_subcommands = NULL;
static int g_subcommand_count = 0, g_subcommand_cap = 0;

/* --- compile_commands.json support --- */
static int g_generate_compile_commands = 0;
typedef struct {
    char *directory;
    char *command;
    char *file;
} compile_commands_entry_t;
static compile_commands_entry_t *g_cc_entries = NULL;
static int g_cc_count = 0, g_cc_cap = 0;

// Public API implementation
void cbuild_enable_compile_commands(int enabled) {
    g_generate_compile_commands = enabled;
}

/* Forward declarations of internal utility functions */
static void cbuild_init();  // initialize defaults
static target_t* cbuild_create_target(const char *name, cbuild_target_type type);
static void ensure_capacity_charpp(char ***arr, int *count, int *capacity);
static void append_str(char **dst, const char *src);
static void append_format(char **dst, const char *fmt, ...);
static int ensure_dir_exists(const char *path);
static int run_command(const char *cmd, int capture_out, char **captured_output);
static int compile_source(const char *src_file, const char *obj_file, const char *dep_file, target_t *t);
static int need_recompile(const char *src_file, const char *obj_file, const char *dep_file);
static int link_target(target_t *t);
static void remove_file(const char *path);
static void remove_dir_recursive(const char *path);
static void schedule_compile_jobs(target_t *t, int *error_flag);
static void build_target(target_t *t, int *error_flag);

/* Structures and funcs for thread pool (for parallel compilation) */
#ifdef _WIN32
    static HANDLE *g_thread_handles = NULL;
    static int g_thread_count = 0;
    static CRITICAL_SECTION g_job_mutex;
#else
    static pthread_t *g_thread_ids = NULL;
    static int g_thread_count = 0;
    static pthread_mutex_t g_job_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static int g_next_job_index = 0;  // index of next compile job to pick
typedef struct { target_t *target; int index; } compile_job_t;
static compile_job_t *g_compile_jobs = NULL;
static int g_compile_job_count = 0;

/* Thread worker function prototype */
#ifdef _WIN32
static DWORD WINAPI compile_thread_func(LPVOID param);
#else
static void* compile_thread_func(void *param);
#endif

/* Helper macro to get max of two values */
#define CBUILD_MAX(a,b) ((a) > (b) ? (a) : (b))

/* --- Implementation: Utility Functions --- */

/* ensure_capacity_charpp: ensure char** array has room for one more element (expand if needed) */
static void ensure_capacity_charpp(char ***arr, int *count, int *capacity) {
    if (*count + 1 > *capacity) {
        int newcap = *capacity ? *capacity * 2 : 4;
        char **newarr = (char**) realloc(*arr, newcap * sizeof(char*));
        if (!newarr) {
            fprintf(stderr, "cbuild: Out of memory\n");
            exit(1);
        }
        *arr = newarr;
        *capacity = newcap;
    }
}

/* append_str: append a copy of string src onto dynamic string *dst (reallocating *dst) */
static void append_str(char **dst, const char *src) {
    if (!src) return;
    size_t src_len = strlen(src);
    size_t old_len = *dst ? strlen(*dst) : 0;
    *dst = (char*) realloc(*dst, old_len + src_len + 1);
    if (!*dst) {
        fprintf(stderr, "cbuild: Out of memory\n");
        exit(1);
    }
    strcpy(*dst + old_len, src);
}

/* append_format: append formatted text to a dynamic string (like a simple asprintf accumulation) */
static void append_format(char **dst, const char *fmt, ...) {
    va_list args;
    va_list args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    int add_len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (add_len < 0) {
        va_end(args);
        return;
    }
    size_t old_len = *dst ? strlen(*dst) : 0;
    *dst = (char*) realloc(*dst, old_len + add_len + 1);
    if (!*dst) {
        fprintf(stderr, "cbuild: Out of memory\n");
        exit(1);
    }
    vsnprintf(*dst + old_len, add_len + 1, fmt, args);
    va_end(args);
}

/* ensure_dir_exists: create a directory (and parent directories) if not present.
   Returns 0 on success, -1 on failure. */
static int ensure_dir_exists(const char *path) {
    if (!path || !*path) return 0;
    // We will create intermediate dirs one by one.
    char temp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) {
        return -1; // path too long
    }
    strcpy(temp, path);
    // Remove trailing slash or backslash if any
    if (temp[len-1] == '/' || temp[len-1] == '\\') {
        temp[len-1] = '\0';
    }
    for (char *p = temp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            // Temporarily truncate at this subdir
            *p = '\0';
#ifdef _WIN32
            if (_mkdir(temp) != 0) {
                if(errno != EEXIST) { *p = '/'; return -1; }
            }
#else
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                *p = '/'; return -1;
            }
#endif
            *p = '/'; // restore separator
        }
    }
    // Create final directory
#ifdef _WIN32
    if (_mkdir(temp) != 0) {
        if(errno != EEXIST) return -1;
    }
#else
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
#endif
    return 0;
}

/* run_command: Runs a shell command. If capture_out is true, captures stdout in *captured_output (must be freed by caller).
   Returns process exit code (0 for success). */
static int run_command(const char *cmd, int capture_out, char **captured_output) {
    if (capture_out) {
        // Open pipe to capture output
#ifdef _WIN32
        // Use _popen on Windows
        FILE *pipe = _popen(cmd, "r");
#else
        FILE *pipe = popen(cmd, "r");
#endif
        if (!pipe) {
            return -1;
        }
        char buffer[256];
        size_t out_len = 0;
        *captured_output = NULL;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            size_t chunk = strlen(buffer);
            *captured_output = (char*) realloc(*captured_output, out_len + chunk + 1);
            if (!*captured_output) {
                fprintf(stderr, "cbuild: Out of memory capturing command output\n");
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                return -1;
            }
            memcpy(*captured_output + out_len, buffer, chunk);
            out_len += chunk;
            (*captured_output)[out_len] = '\0';
        }
#ifdef _WIN32
        int exitCode = _pclose(pipe);
#else
        int exitCode = pclose(pipe);
#endif
        // Normalize exit code (on Windows, _pclose returns -1 if error launching)
        if (exitCode == -1) {
            return -1;
        }
        // On Windows, exitCode is the result of cmd << 8 (conventional), so 0 means success.
        // We'll just return as is, since for our usage 0 indicates success.
        return exitCode;
    } else {
        // Not capturing output: use system() to execute (prints output directly).
        int ret = system(cmd);
        // system returns encoded status; we assume 0 means success on both Windows and Unix.
        return ret;
    }
}

/* need_recompile: Checks timestamps of source, object, and included headers to decide if recompilation is needed.
   Returns 1 if the source (or its headers) is newer than object (or object missing), otherwise 0. */
static int need_recompile(const char *src_file, const char *obj_file, const char *dep_file) {
    struct stat st_src, st_obj;
    if (stat(src_file, &st_src) != 0) {
        return 1;
    }
    if (stat(obj_file, &st_obj) != 0) {
        return 1;
    }
    if (st_src.st_mtime > st_obj.st_mtime) {
        return 1;
    }
    // Only check dep file if dependency tracking is enabled
    if (g_dep_tracking) {
        FILE *dep = fopen(dep_file, "r");
        if (dep) {
            char linebuf[1024];
            char *deps = NULL;
            size_t deps_len = 0;
            while (fgets(linebuf, sizeof(linebuf), dep)) {
                size_t len = strlen(linebuf);
                if (len > 0 && linebuf[len-1] == '\n') {
                    linebuf[len-1] = '\0';
                    --len;
                }
                int continued = 0;
                if (len > 0 && linebuf[len-1] == '\\') {
                    continued = 1;
                    linebuf[len-1] = '\0';
                    --len;
                }
                deps = (char*) realloc(deps, deps_len + len + 1);
                if (!deps) {
                    fclose(dep);
                    return 1;
                }
                memcpy(deps + deps_len, linebuf, len);
                deps_len += len;
                deps[deps_len] = '\0';
                if (!continued) {
                }
            }
            fclose(dep);
            if (deps) {
                char *colon = strchr(deps, ':');
                if (colon) {
                    colon++;
                    while (*colon == ' ' || *colon == '\t') colon++;
                } else {
                    colon = deps;
                }
                char *token = colon;
                while (*token) {
                    if (*token == ' ' || *token == '\t') {
                        token++;
                        continue;
                    }
                    if (*token == '\\') {
                        if (*(token+1) == ' ') {
                            memmove(token, token+1, strlen(token));
                            continue;
                        }
                    }
                    char *end = token;
                    while (*end && *end != ' ' && *end != '\t') {
                        if (*end == '\\' && *(end+1) == ' ') {
                            end++;
                        }
                        end++;
                    }
                    char saved = *end;
                    *end = '\0';
                    const char *dep_path = token;
                    if (strcmp(dep_path, src_file) != 0) {
                        struct stat st_dep;
                        if (stat(dep_path, &st_dep) == 0) {
                            if (st_dep.st_mtime > st_obj.st_mtime) {
                                free(deps);
                                return 1;
                            }
                        } else {
                            free(deps);
                            return 1;
                        }
                    }
                    *end = saved;
                    token = end;
                }
                free(deps);
            }
        }
    }
    return 0;
}

/* compile_source: Compile a single source file to object file (and produce dep file).
   Returns 0 on success, non-zero on failure. */
static int compile_source(const char *src_file, const char *obj_file, const char *dep_file, target_t *t) {
    ensure_dir_exists(t->obj_dir);
    char *cmd = NULL;
    append_format(&cmd, "\"%s\" ", g_cc);
#ifdef _WIN32
    append_format(&cmd, "/c /nologo /Fo\"%s\" ", obj_file);
    append_str(&cmd, "/showIncludes ");
#else
    append_format(&cmd, "-c -o \"%s\" ", obj_file);
    // Only add dependency flags if enabled
    if (g_dep_tracking) {
        append_format(&cmd, "-MMD -MF \"%s\" ", dep_file);
    }
#endif
    if (g_global_cflags) {
        append_format(&cmd, "%s ", g_global_cflags);
    }
    if (t->cflags) {
        append_format(&cmd, "%s ", t->cflags);
    }
    for (int i = 0; i < t->include_count; ++i) {
        const char *inc = t->include_dirs[i];
#ifdef _WIN32
        append_format(&cmd, "/I \"%s\" ", inc);
#else
        append_format(&cmd, "-I\"%s\" ", inc);
#endif
    }
    append_format(&cmd, "\"%s\"", src_file);

    // Record compile command if enabled
    if (g_generate_compile_commands) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            if (g_cc_count + 1 > g_cc_cap) {
                g_cc_cap = g_cc_cap ? g_cc_cap * 2 : 4;
                g_cc_entries = realloc(g_cc_entries, g_cc_cap * sizeof(*g_cc_entries));
            }
            g_cc_entries[g_cc_count].directory = strdup(cwd);
            g_cc_entries[g_cc_count].command   = strdup(cmd);
            g_cc_entries[g_cc_count].file      = strdup(src_file);
            g_cc_count++;
        }
    }

    int result;
    char *output = NULL;
#ifdef _WIN32
    result = run_command(cmd, 1, &output);
    if (output) {
        FILE *df = fopen(dep_file, "w");
        if (df) {
            fprintf(df, "%s: %s", obj_file, src_file);
            char *saveptr = NULL;
            char *line = strtok_r(output, "\r\n", &saveptr);
            while (line) {
                const char *tag = "Note: including file:";
                char *pos = strstr(line, tag);
                if (pos) {
                    pos += strlen(tag);
                    while (*pos == ' ' || *pos == '\t') pos++;
                    if (*pos) {
                        fprintf(df, " \\\n  %s", pos);
                    }
                }
                line = strtok_r(NULL, "\r\n", &saveptr);
            }
            fprintf(df, "\n");
            fclose(df);
        }
        if (result != 0) {
            fwrite(output, 1, strlen(output), stderr);
        }
        free(output);
    }
#else
    result = run_command(cmd, 1, &output);
    if (output && result != 0) {
        fwrite(output, 1, strlen(output), stderr);
    }
    if (output) free(output);
#endif
    free(cmd);
    if (result != 0) {
        fprintf(stderr, "cbuild: Compilation failed for %s\n", src_file);
    }
    return result;
}

/* --- Implementation: Command API --- */

command_t* cbuild_command(const char *name, const char *command_line) {
    command_t *cmd = (command_t*)calloc(1, sizeof(command_t));
    cmd->name = strdup(name);
    cmd->command_line = strdup(command_line);
    // Add to global command list
    ensure_capacity_charpp((char***)&g_commands, &g_command_count, &g_command_cap);
    g_commands[g_command_count++] = cmd;
    return cmd;
}

void cbuild_target_add_command(target_t *target, command_t *cmd) {
    if (!target || !cmd) return;
    ensure_capacity_charpp((char***)&target->commands, &target->cmd_count, &target->cmd_cap);
    target->commands[target->cmd_count++] = cmd;
}

void cbuild_target_add_post_command(target_t *target, command_t *cmd) {
    if (!target || !cmd) return;
    ensure_capacity_charpp((char***)&target->post_commands, &target->post_cmd_count, &target->post_cmd_cap);
    target->post_commands[target->post_cmd_count++] = cmd;
}

void cbuild_command_add_dependency(command_t *cmd, command_t *dependency) {
    if (!cmd || !dependency) return;
    ensure_capacity_charpp((char***)&cmd->dependencies, &cmd->dep_count, &cmd->dep_cap);
    cmd->dependencies[cmd->dep_count++] = dependency;
}

int cbuild_run_command(command_t *cmd) {
    if (!cmd) return -1;
    // Run dependencies first
    for (int i = 0; i < cmd->dep_count; ++i) {
        int rc = cbuild_run_command(cmd->dependencies[i]);
        if (rc != 0) return rc;
    }
    if (cmd->executed) return cmd->result;
    cbuild_pretty_step("COMMAND", CBUILD_COLOR_MAGENTA, "%s", cmd->name);
    int rc = run_command(cmd->command_line, 0, NULL);
    cmd->executed = 1;
    cmd->result = rc;
    if (rc != 0) {
        cbuild_pretty_status(0, "Command failed: %s", cmd->name);
    }
    return rc;
}

/* --- Implementation: Subcommand API --- */

void cbuild_register_subcommand(const char *name, target_t *target, const char *command_line, cbuild_subcommand_callback callback, void *user_data) {
    cbuild_subcommand_t *scmd = (cbuild_subcommand_t*)calloc(1, sizeof(cbuild_subcommand_t));
    scmd->name = strdup(name);
    scmd->target = target;
    scmd->command_line = command_line ? strdup(command_line) : NULL;
    scmd->callback = callback;
    scmd->user_data = user_data;
    ensure_capacity_charpp((char***)&g_subcommands, &g_subcommand_count, &g_subcommand_cap);
    g_subcommands[g_subcommand_count++] = scmd;
}

/* --- Implementation: Public API Functions --- */

target_t* cbuild_executable(const char *name) {
    return cbuild_create_target(name, TARGET_EXECUTABLE);
}

target_t* cbuild_static_library(const char *name) {
    return cbuild_create_target(name, TARGET_STATIC_LIB);
}

target_t* cbuild_shared_library(const char *name) {
    return cbuild_create_target(name, TARGET_SHARED_LIB);
}

void cbuild_add_source(target_t *target, const char *source_file) {
    ensure_capacity_charpp(&target->sources, &target->sources_count, &target->sources_cap);
    target->sources[target->sources_count++] = strdup(source_file);
}

void cbuild_add_include_dir(target_t *target, const char *include_path) {
    ensure_capacity_charpp(&target->include_dirs, &target->include_count, &target->include_cap);
    target->include_dirs[target->include_count++] = strdup(include_path);
}

void cbuild_add_library_dir(target_t *target, const char *lib_dir) {
    ensure_capacity_charpp(&target->lib_dirs, &target->lib_dir_count, &target->lib_dir_cap);
    target->lib_dirs[target->lib_dir_count++] = strdup(lib_dir);
}

void cbuild_add_link_library(target_t *target, const char *lib_name) {
    ensure_capacity_charpp(&target->link_libs, &target->link_lib_count, &target->link_lib_cap);
    target->link_libs[target->link_lib_count++] = strdup(lib_name);
}

void cbuild_target_link_library(target_t *dependant, target_t *dependency) {
    if (dependency) {
        ensure_capacity_charpp((char***)&dependant->dependencies, &dependant->dep_count, &dependant->dep_cap);
        dependant->dependencies[dependant->dep_count++] = dependency;
    }
}

void cbuild_set_output_dir(const char *dir) {
    if (g_output_dir) {
        free(g_output_dir);
    }
    g_output_dir = strdup(dir);
}

void cbuild_set_parallelism(int jobs_count) {
    g_parallel_jobs = jobs_count;
}

void cbuild_set_compiler(const char *compiler_exe) {
    if (g_cc) free(g_cc);
    g_cc = strdup(compiler_exe);
    if (strstr(compiler_exe, "cl") != NULL && strstr(compiler_exe, "clang") == NULL) {
        if (g_ar) free(g_ar);
        g_ar = strdup("lib");
    } else if (strstr(compiler_exe, "cl") == NULL) {
        if (g_ar) free(g_ar);
        g_ar = strdup("ar");
    }
}

void cbuild_add_global_cflags(const char *flags) {
    append_format(&g_global_cflags, "%s ", flags);
}

void cbuild_add_global_ldflags(const char *flags) {
    append_format(&g_global_ldflags, "%s ", flags);
}

void cbuild_enable_dep_tracking(int enabled) {
    g_dep_tracking = enabled;
}

/* Static variables for DFS build function */
static int *visited = NULL;
static int *in_stack = NULL;

/* DFS build function (extended to handle commands as dependencies) */
static void dfs_command_func(command_t *cmd, int *error_flag_ptr) {
    if (!cmd || *error_flag_ptr) return;
    if (cmd->executed) return;
    for (int i = 0; i < cmd->dep_count; ++i) {
        dfs_command_func(cmd->dependencies[i], error_flag_ptr);
        if (*error_flag_ptr) return;
    }
    if (cbuild_run_command(cmd) != 0) {
        *error_flag_ptr = 1;
    }
}

static void dfs_build_func(target_t *t, int *error_flag_ptr) {
    int ti = -1;
    for (int j = 0; j < g_target_count; ++j) {
        if (g_targets[j] == t) { ti = j; break; }
    }
    if (ti == -1) return;
    if (*error_flag_ptr) return;
    if (in_stack[ti]) {
        fprintf(stderr, "cbuild: Error - circular dependency involving %s\n", t->name);
        *error_flag_ptr = 1;
        return;
    }
    if (visited[ti]) return;
    in_stack[ti] = 1;
    // Run command dependencies first
    for (int ci = 0; ci < t->cmd_count; ++ci) {
        dfs_command_func(t->commands[ci], error_flag_ptr);
        if (*error_flag_ptr) {
            in_stack[ti] = 0;
            return;
        }
    }
    // Then build target dependencies
    for (int di = 0; di < t->dep_count; ++di) {
        dfs_build_func(t->dependencies[di], error_flag_ptr);
        if (*error_flag_ptr) {
            in_stack[ti] = 0;
            return;
        }
    }
    build_target(t, error_flag_ptr);
    // Run post-build commands
    for (int pci = 0; pci < t->post_cmd_count; ++pci) {
        dfs_command_func(t->post_commands[pci], error_flag_ptr);
        if (*error_flag_ptr) {
            in_stack[ti] = 0;
            return;
        }
    }
    visited[ti] = 1;
    in_stack[ti] = 0;
}

// Helper to write a JSON string with proper escaping
static void fprint_json_string(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; ++s) {
        switch (*s) {
            case '\\': fputs("\\\\", f); break;
            case '"':  fputs("\\\"", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if ((unsigned char)*s < 0x20) {
                    fprintf(f, "\\u%04x", (unsigned char)*s);
                } else {
                    fputc(*s, f);
                }
        }
    }
    fputc('"', f);
}

int cbuild_run(int argc, char **argv) {
    cbuild_init();
    if (argc > 1) {
        if (strcmp(argv[1], "clean") == 0) {
            cbuild_pretty_step("CLEAN", CBUILD_COLOR_YELLOW, "Cleaning build outputs...");
            for (int i = 0; i < g_target_count; ++i) {
                target_t *t = g_targets[i];
                remove_dir_recursive(t->obj_dir);
                remove_file(t->output_file);
            }
            remove_dir_recursive(g_output_dir);
            cbuild_pretty_status(1, "Clean complete.");
            return 0;
        }
        // Check for custom subcommands
        for (int sci = 0; sci < g_subcommand_count; ++sci) {
            cbuild_subcommand_t *scmd = g_subcommands[sci];
            if (strcmp(argv[1], scmd->name) == 0) {
                // Build the dependency target first
                int error_flag = 0;
                // Allocate and clear visited/in_stack arrays
                if (visited) free(visited);
                if (in_stack) free(in_stack);
                visited = calloc(g_target_count, sizeof(int));
                in_stack = calloc(g_target_count, sizeof(int));
                dfs_build_func(scmd->target, &error_flag);
                free(visited); visited = NULL;
                free(in_stack); in_stack = NULL;
                if (error_flag) {
                    cbuild_pretty_status(0, "Build failed.");
                    return 1;
                }
                // Run the subcommand
                int rc = 0;
                if (scmd->command_line) {
                    cbuild_pretty_step("SUBCMD", CBUILD_COLOR_BLUE, "Running '%s': %s", scmd->name, scmd->command_line);
                    rc = run_command(scmd->command_line, 0, NULL);
                } else if (scmd->callback) {
                    cbuild_pretty_step("SUBCMD", CBUILD_COLOR_BLUE, "Running '%s' (callback)...", scmd->name);
                    scmd->callback(scmd->user_data);
                }
                return rc;
            }
        }
    }
    int error_flag = 0;
    // Allocate and clear visited/in_stack arrays
    if (visited) free(visited);
    if (in_stack) free(in_stack);
    visited = calloc(g_target_count, sizeof(int));
    in_stack = calloc(g_target_count, sizeof(int));

    for (int i = 0; i < g_target_count; ++i) {
        if (!visited[i]) {
            dfs_build_func(g_targets[i], &error_flag);
            if (error_flag) break;
        }
    }
    free(visited); visited = NULL;
    free(in_stack); in_stack = NULL;
    if (!error_flag) {
        // Dump compile_commands.json if enabled
        if (g_generate_compile_commands) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/compile_commands.json", g_output_dir);
            FILE *f = fopen(path, "w");
            if (f) {
                fprintf(f, "[\n");
                for (int i = 0; i < g_cc_count; i++) {
                    fprintf(f, "  {\"directory\":");
                    fprint_json_string(f, g_cc_entries[i].directory);
                    fprintf(f, ",\"command\":");
                    fprint_json_string(f, g_cc_entries[i].command);
                    fprintf(f, ",\"file\":");
                    fprint_json_string(f, g_cc_entries[i].file);
                    fprintf(f, "}%s\n", (i + 1 < g_cc_count) ? "," : "");
                }
                fprintf(f, "]\n");
                fclose(f);
            }
        }
        cbuild_pretty_status(1, "Build succeeded.");
        return 0;
    } else {
        cbuild_pretty_status(0, "Build failed.");
        return 1;
    }
}

// Initialize global/default build settings
static void cbuild_init() {
    if (!g_output_dir) g_output_dir = strdup("build");
    if (!g_cc) g_cc = strdup("cc");
    if (!g_ar) g_ar = strdup("ar");
#if defined(_WIN32)
    if (!g_ld) g_ld = strdup("ld");
#elif defined(__APPLE__) || defined(__linux__)
    if (!g_ld) g_ld = strdup(g_cc); // Use compiler as linker on macOS/Linux
#else
    if (!g_ld) g_ld = strdup("ld");
#endif
    if (g_parallel_jobs <= 0) {
        // Try to detect CPU count
        int n = 1;
#ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        n = sysinfo.dwNumberOfProcessors;
#else
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpus > 0) n = (int)cpus;
#endif
        g_parallel_jobs = n > 0 ? n : 1;
    }
}

// Create a new target struct and add to global list
static target_t* cbuild_create_target(const char *name, cbuild_target_type type) {
    target_t *t = (target_t*)calloc(1, sizeof(target_t));
    t->type = type;
    t->name = strdup(name);
    // Set output file and obj_dir
    char *out = NULL, *obj = NULL;
    if (type == TARGET_EXECUTABLE) {
#ifdef _WIN32
        append_format(&out, "%s/%s.exe", g_output_dir, name);
#else
        append_format(&out, "%s/%s", g_output_dir, name);
#endif
    } else if (type == TARGET_STATIC_LIB) {
#ifdef _WIN32
        append_format(&out, "%s/%s.lib", g_output_dir, name);
#else
        append_format(&out, "%s/lib%s.a", g_output_dir, name);
#endif
    } else if (type == TARGET_SHARED_LIB) {
#ifdef _WIN32
        append_format(&out, "%s/%s.dll", g_output_dir, name);
#elif __APPLE__
        append_format(&out, "%s/lib%s.dylib", g_output_dir, name);
#else
        append_format(&out, "%s/lib%s.so", g_output_dir, name);
#endif
    }
    append_format(&obj, "%s/obj_%s", g_output_dir, name);
    t->output_file = out;
    t->obj_dir = obj;
    t->commands = NULL;
    t->cmd_count = t->cmd_cap = 0;
    t->post_commands = NULL;
    t->post_cmd_count = t->post_cmd_cap = 0;
    // Add to global list
    ensure_capacity_charpp((char***)&g_targets, &g_target_count, &g_target_cap);
    g_targets[g_target_count++] = t;
    return t;
}

// Remove a file from disk
static void remove_file(const char *path) {
    if (!path || !*path) return;
    remove(path);
}

// Recursively remove a directory and its contents
static void remove_dir_recursive(const char *path) {
    if (!path || !*path) return;
#ifdef _WIN32
    WIN32_FIND_DATA ffd;
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    HANDLE hFind = FindFirstFile(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", path, ffd.cFileName);
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            remove_dir_recursive(full);
        } else {
            remove_file(full);
        }
    } while (FindNextFile(hFind, &ffd));
    FindClose(hFind);
    _rmdir(path);
#else
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    char buf[1024];
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);
        struct stat st;
        if (!stat(buf, &st)) {
            if (S_ISDIR(st.st_mode)) {
                remove_dir_recursive(buf);
            } else {
                remove_file(buf);
            }
        }
    }
    closedir(dir);
    rmdir(path);
#endif
}

// Build a target (compile sources, link, etc.)
static void build_target(target_t *t, int *error_flag) {
    // Compile all sources to objects
    int obj_count = t->sources_count;
    char **obj_files = (char**)calloc(obj_count, sizeof(char*));
    for (int i = 0; i < obj_count; ++i) {
        const char *src = t->sources[i];
        const char *slash = strrchr(src, '/');
        const char *base = slash ? slash + 1 : src;
        char *dot = strrchr(base, '.');
        size_t len = dot ? (size_t)(dot - base) : strlen(base);
        char objname[512];
        snprintf(objname, sizeof(objname), "%s/%.*s.o", t->obj_dir, (int)len, base);
        obj_files[i] = strdup(objname);

        char depname[512];
        snprintf(depname, sizeof(depname), "%s/%.*s.o.d", t->obj_dir, (int)len, base);

        if (need_recompile(src, objname, depname)) {
            cbuild_pretty_step("COMPILE", CBUILD_COLOR_BLUE, "%s", src);
            if (compile_source(src, objname, depname, t) != 0) {
                *error_flag = 1;
                goto cleanup;
            }
        }
    }

    // Link if needed
    int needs_link = 0;
    struct stat st_out;
    if (stat(t->output_file, &st_out) != 0) {
        needs_link = 1;
    } else {
        // Check if any object file is newer than the output
        for (int i = 0; i < obj_count; ++i) {
            struct stat st_obj;
            if (stat(obj_files[i], &st_obj) != 0 || st_obj.st_mtime > st_out.st_mtime) {
                needs_link = 1;
                break;
            }
        }
        // Check if any dependency output is newer than the output
        if (!needs_link) {
            for (int i = 0; i < t->dep_count; ++i) {
                target_t *dep = t->dependencies[i];
                if (dep->output_file) {
                    struct stat st_dep;
                    if (stat(dep->output_file, &st_dep) == 0) {
                        if (st_dep.st_mtime > st_out.st_mtime) {
                            needs_link = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (needs_link) {
        cbuild_pretty_step("LINK", CBUILD_COLOR_YELLOW, "%s", t->output_file);
        char *cmd = NULL;
        if (t->type == TARGET_STATIC_LIB) {
#ifdef _WIN32
            append_format(&cmd, "%s /OUT:%s", g_ar, t->output_file);
            for (int i = 0; i < obj_count; ++i) append_format(&cmd, " %s", obj_files[i]);
#else
            append_format(&cmd, "%s rcs %s", g_ar, t->output_file);
            for (int i = 0; i < obj_count; ++i) append_format(&cmd, " %s", obj_files[i]);
#endif
        } else if (t->type == TARGET_EXECUTABLE || t->type == TARGET_SHARED_LIB) {
            append_format(&cmd, "%s -o %s", g_ld, t->output_file);
            for (int i = 0; i < obj_count; ++i) append_format(&cmd, " %s", obj_files[i]);
            for (int i = 0; i < t->lib_dir_count; ++i)
#ifdef _WIN32
                append_format(&cmd, " /LIBPATH:\"%s\"", t->lib_dirs[i]);
#else
                append_format(&cmd, " -L\"%s\"", t->lib_dirs[i]);
#endif
            for (int i = 0; i < t->link_lib_count; ++i)
#ifdef _WIN32
                append_format(&cmd, " %s.lib", t->link_libs[i]);
#elif __APPLE__
                append_format(&cmd, " -l%s.dylib", t->link_libs[i]);
#else
                append_format(&cmd, " -l%s", t->link_libs[i]);
#endif
            for (int i = 0; i < t->dep_count; ++i) {
                target_t *dep = t->dependencies[i];
                if (dep->type == TARGET_STATIC_LIB || dep->type == TARGET_SHARED_LIB)
                    append_format(&cmd, " %s", dep->output_file);
            }
            if (t->ldflags) append_format(&cmd, " %s", t->ldflags);
            if (g_global_ldflags) append_format(&cmd, " %s", g_global_ldflags);
            if (t->type == TARGET_SHARED_LIB) {
#ifdef _WIN32
                append_format(&cmd, " /DLL");
#else
                append_format(&cmd, " -shared");
#endif
            }
        }
        int rc;
        char *output = NULL;
        rc = run_command(cmd, 1, &output);
        if (output && rc != 0) {
            fwrite(output, 1, strlen(output), stderr);
        }
        if (output) free(output);
        free(cmd);
        if (rc != 0) {
            cbuild_pretty_status(0, "Linking failed for %s", t->output_file);
            *error_flag = 1;
            goto cleanup;
        }
    }

cleanup:
    for (int i = 0; i < obj_count; ++i) free(obj_files[i]);
    free(obj_files);
}

#endif /* CBUILD_IMPLEMENTATION */
