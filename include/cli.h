#ifndef CLI_H
#define CLI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Public types
typedef enum {
    CLI_TYPE_FLAG,
    CLI_TYPE_INT,
    CLI_TYPE_STRING,
    CLI_TYPE_BOOL,
} cli_type;

typedef struct cli_opt {
    char short_name;
    const char *long_name;
    cli_type type;
    void *dest;
    const char *default_val;
    const char *help;
} cli_opt;

// Public macros for defining options
#define CLI_BEGIN(name, argc, argv) \
    cli_opt name##_opts[] = {

#define CLI_FLAG(short_name_val, long_name_val, var_val, help_val) \
    { short_name_val, long_name_val, CLI_TYPE_FLAG, &var_val, (const char *)0, help_val },

#define CLI_BOOL(short_name_val, long_name_val, var_val, help_val) \
    { short_name_val, long_name_val, CLI_TYPE_BOOL, &var_val, (const char *)0, help_val },

#define CLI_INT(short_name_val, long_name_val, var_val, default_val_val, help_val) \
    { short_name_val, long_name_val, CLI_TYPE_INT, &var_val, #default_val_val, help_val },

#define CLI_STRING(short_name_val, long_name_val, var_val, default_val_val, help_val) \
    { short_name_val, long_name_val, CLI_TYPE_STRING, &var_val, default_val_val, help_val },

#define CLI_END(name) \
    }; \
    cli_parse(name##_opts, sizeof(name##_opts) / sizeof(name##_opts[0]), argc, argv);

// Public function declarations
void cli_parse(cli_opt *opts, size_t opt_count, int argc, char *argv[]);
void cli_print_help(cli_opt *opts, size_t opt_count, const char *prog_name);

#ifdef CLI_IMPLEMENTATION
// Public function implementations
void cli_parse(cli_opt *opts, size_t opt_count, int argc, char *argv[]) {
    int i = 1;

    // Set defaults
    for (size_t n = 0; n < opt_count; ++n) {
        cli_opt *opt = &opts[n];
        switch (opt->type) {
            case CLI_TYPE_FLAG:
                *(int *)opt->dest = 0;
                break;
            case CLI_TYPE_BOOL:
                *(int *)opt->dest = 0;
                break;
            case CLI_TYPE_INT:
                if (opt->default_val)
                    *(int *)opt->dest = atoi(opt->default_val);
                break;
            case CLI_TYPE_STRING:
                if (opt->default_val)
                    *(const char **)opt->dest = opt->default_val;
                else
                    *(const char **)opt->dest = NULL;
                break;
        }
    }

    while (i < argc) {
        char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            cli_print_help(opts, opt_count, argv[0]);
            exit(0);
        }

        if (arg[0] != '-') {
            // Non-option argument, skip (not supported in this lib)
            fprintf(stderr, "Ignoring unexpected argument: %s\n", arg);
            ++i;
            continue;
        }

        int is_long = (arg[1] == '-');
        char short_name = is_long ? 0 : arg[1];
        const char *long_name = is_long ? &arg[2] : NULL;

        // Handle `--key=value` syntax
        const char *value = NULL;
        if (is_long && (value = strchr(long_name, '=')) != NULL) {
            size_t len = value - long_name;
            char key[len + 1];
            strncpy(key, long_name, len);
            key[len] = '\0';
            long_name = key;
            value += 1;
        }

        int found = 0;
        for (size_t n = 0; n < opt_count; ++n) {
            cli_opt *opt = &opts[n];

            if ((is_long && opt->long_name && strcmp(long_name, opt->long_name) == 0) ||
                (!is_long && opt->short_name == short_name)) {

                found = 1;

                switch (opt->type) {
                    case CLI_TYPE_FLAG:
                    case CLI_TYPE_BOOL:
                        *(int *)opt->dest = 1;
                        ++i;
                        break;

                    case CLI_TYPE_INT:
                    case CLI_TYPE_STRING: {
                        if (!value) {
                            // Value is in next argument
                            if (++i >= argc) {
                                fprintf(stderr, "Option %s requires a value\n", is_long ? long_name : &arg[1]);
                                cli_print_help(opts, opt_count, argv[0]);
                                exit(1);
                            }
                            value = argv[i++];
                        }

                        if (opt->type == CLI_TYPE_INT) {
                            *(int *)opt->dest = atoi(value);
                        } else if (opt->type == CLI_TYPE_STRING) {
                            *(const char **)opt->dest = value;
                        }
                        break;
                    }

                    default:
                        break;
                }
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Unknown option: %s\n", arg);
            cli_print_help(opts, opt_count, argv[0]);
            exit(1);
        }
    }
}

void cli_print_help(cli_opt *opts, size_t opt_count, const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");

    for (size_t i = 0; i < opt_count; ++i) {
        cli_opt *opt = &opts[i];
        printf("  ");
        if (opt->short_name)
            printf("-%c, ", opt->short_name);
        else
            printf("    ");
        if (opt->long_name)
            printf("--%-15s", opt->long_name);
        else
            printf("    ");

        switch (opt->type) {
            case CLI_TYPE_FLAG:
            case CLI_TYPE_BOOL:
                printf("        ");
                break;
            case CLI_TYPE_INT:
            case CLI_TYPE_STRING:
                printf(" <arg>  ");
                break;
            default:
                break;
        }

        if (opt->default_val)
            printf("[default: %s] ", opt->default_val);
        printf("%s\n", opt->help);
    }
}

#endif // CLI_IMPLEMENTATION
#endif // CLI_H
