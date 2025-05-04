#define CBUILD_IMPLEMENTATION
#include "cbuild.h"
#include <stdbool.h>

bool check_deps() {
    // check if the directory ./vendor/zlib exists
    if (access("./vendor/zlib", F_OK) == -1) {
        // directory does not exist, return false
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    CBUILD_SELF_REBUILD("build.c", "cbuild.h");

    if (!check_deps()) {
        // make the vendor directory if it does not exist
        if (mkdir("./vendor", 0755) == -1) {
            fprintf(stderr, "Failed to create vendor directory: %s\n", strerror(errno));
            return 1;
        }
        printf("Dependencies not found, downloading...\n");
        // run the command to download the dependencies
        command_t* cmd = cbuild_command("download deps", "./scripts/download.sh");
        if (cmd) {
            int ret = cbuild_run_command(cmd);
            if (ret != 0) {
                fprintf(stderr, "Failed to download dependencies: %d\n", ret);
                return ret;
            }
        } else {
            fprintf(stderr, "Failed to create command to download dependencies\n");
            return 1;
        }
        printf("Dependencies downloaded.\n");
    }

    cbuild_set_output_dir("build");
    cbuild_set_compiler("gcc");
    cbuild_enable_compile_commands(1);
#ifdef __APPLE__
    cbuild_add_global_cflags("-std=c2x -Wall -Wextra -Wpedantic -Werror");
#else
    cbuild_add_global_cflags("-std=gnu23 -Wall -Wextra -Wpedantic -Werror");
#endif

    target_t* zlib;
    CBUILD_STATIC_LIBRARY(zlib,
        CBUILD_SOURCES(zlib, "vendor/zlib/adler32.c",
            "vendor/zlib/compress.c", "vendor/zlib/crc32.c",
            "vendor/zlib/deflate.c", "vendor/zlib/gzlib.c",
            "vendor/zlib/gzread.c", "vendor/zlib/gzwrite.c",
            "vendor/zlib/infback.c", "vendor/zlib/inffast.c",
            "vendor/zlib/inflate.c", "vendor/zlib/inftrees.c",
            "vendor/zlib/trees.c", "vendor/zlib/uncompr.c",
            "vendor/zlib/zutil.c");
        CBUILD_INCLUDES(zlib, "vendor/zlib");
    );

    // build the http library
    target_t* http;
    CBUILD_STATIC_LIBRARY(http,
        CBUILD_SOURCES(http, "src/http.c", "src/router.c", "src/routes.c",
            "src/layers.c", "src/alloc.c", "src/cstring.c", "src/utils.c",
            "src/builtin.c", "src/server.c");
        CBUILD_INCLUDES(http, "include", "vendor/zlib");
    );
    cbuild_target_link_library(http, zlib);

    // build the http server executable
    target_t* server;
    CBUILD_EXECUTABLE(server,
        CBUILD_SOURCES(server, "src/main.c");
        CBUILD_INCLUDES(server, "include");
    );

    // link the http server to the zlib library and the http library
    cbuild_target_link_library(server, zlib);
    cbuild_target_link_library(server, http);

    command_t* copy_cmd = cbuild_command("copy server exe to root", "cp ./build/server server");
    cbuild_target_add_post_command(server, copy_cmd);

    cbuild_register_subcommand("run", server, "./server -v -d .", NULL, NULL);
    cbuild_register_subcommand("submit", NULL, "./scripts/submit.sh", NULL, NULL);
    cbuild_register_subcommand("vendor", NULL, "./scripts/download.sh", NULL, NULL);

    return cbuild_run(argc, argv);
}
