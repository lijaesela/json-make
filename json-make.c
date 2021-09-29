#include <json-c/json.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

// macro fan vs. goto enjoyer
char *slurp_file(const char *file_path)
{
    FILE *f = fopen(file_path, "r");
    if (f == NULL) goto slurp_file_panic;
    if (fseek(f, 0, SEEK_END) < 0) goto slurp_file_panic;

    long size = ftell(f);
    if (size < 0) goto slurp_file_panic;

    char *buffer = malloc(size + 1);
    if (buffer == NULL) goto slurp_file_panic;

    if (fseek(f, 0, SEEK_SET) < 0) goto slurp_file_panic;

    fread(buffer, 1, size, f);
    if (ferror(f) < 0) goto slurp_file_panic;

    buffer[size] = '\0';

    if (fclose(f) < 0) goto slurp_file_panic;

    return buffer;

slurp_file_panic:
    fprintf(stderr, "Could not read file `%s`: %s\n", file_path, strerror(errno));
    return NULL;
}

int main(void)
{
    char *cc_text = NULL;
    json_object *cc_json = NULL;

    cc_text = slurp_file("compile_commands.json");
    if (!cc_text) {
        fprintf(stderr, "json-make [ERROR]: could not open `compile_commands.json`. does it exist?\n");
        goto main_panic;
    }

    cc_json = json_tokener_parse(cc_text);
    free(cc_text);
    if (!cc_json) {
        fprintf(stderr, "json-make [ERROR]: failed to parse `compile_commands.json`.\n");
        goto main_panic;
    }

    if (json_object_get_type(cc_json) != json_type_array) {
        fprintf(stderr, "json-make [ERROR]: expected type array while parsing. this is not a valid compile_commands.json file.\n");
        goto main_panic;
    }

    char original_directory[4096];
    getcwd(original_directory, sizeof(original_directory));

    size_t action_count = json_object_array_length(cc_json);
    for (size_t action_index = 0; action_index < action_count; ++action_index) {

        const struct json_object *action = json_object_array_get_idx(cc_json, action_index);

        const char *directory = NULL,
                   *file      = NULL,
                   *command   = NULL;

        if (json_object_get_type(action) != json_type_object) {
            fprintf(stderr, "json-make [ERROR]: expected type object while parsing. this is not a valid compile_commands.json file.\n");
            goto main_panic;
        }

        json_object_object_foreach(action, key, val) {
            if /*  */ (strcmp(key, "directory") == 0) {
                directory = json_object_get_string(val);
            } else if (strcmp(key, "file") == 0) {
                file = json_object_get_string(val);
            } else if (strcmp(key, "command") == 0) {
                command = json_object_get_string(val);
            } else {
                fprintf(stderr, "json-make [WARN]: ignoring unknown key \"%s\".\n", key);
            }
        }

        bool error = false;
        if (!directory) {
            fprintf(stderr,
                    "json-make [ERROR]: did not find necessary key \"%s\" for action %zu.\n",
                    "directory", action_index);
            error = true;
        }
        if (!file) {
            fprintf(stderr,
                    "json-make [ERROR]: did not find necessary key \"%s\" for action %zu.\n",
                    "file", action_index);
            error = true;
        }
        if (!command) {
            fprintf(stderr,
                    "json-make [ERROR]: did not find necessary key \"%s\" for action %zu.\n",
                    "command", action_index);
            error = true;
        }
        if (error) goto main_panic;

        // actually do the thing now

        if (strcmp(directory, ".") == 0 || strcmp(directory, "./") == 0) {
            chdir(original_directory);
            fprintf(stderr,
                    "json-make [INFO]: in current directory \"%s\"\n",
                    original_directory);
        } else {
            chdir(directory);
            fprintf(stderr,
                    "json-make [INFO]: changed directory to \"%s\"\n",
                    directory);
        }

        fprintf(stderr, "json-make [CMD]: %s\n", command);
        int cmd_exit_code = system(command);
        if (cmd_exit_code < 0) {
            fprintf(stderr, "json-make [ERROR]: `system()` failed to create a child process.\n");
            goto main_panic;
        } else if (cmd_exit_code > 0) {
            fprintf(stderr, "json-make [ERROR]: command terminated with non-zero exit code.\n");
            goto main_panic;
        }
    }

    exit(0);

main_panic:
    fprintf(stderr, "aborting due to fatal error.\n");
    if (cc_json) {
        free(cc_json);
    }
    exit(1);
}
