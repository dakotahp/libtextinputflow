#define _POSIX_C_SOURCE 200809L

#include "paths.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BINDINGS_SUFFIX "/textinputflow/bindings"

static bool concat(const char *head, size_t head_len, const char *tail, char **out)
{
    size_t tail_len = strlen(tail);
    *out = malloc(head_len + tail_len + 1);
    if (!*out)
        return false;
    memcpy(*out, head, head_len);
    memcpy(*out + head_len, tail, tail_len + 1);
    return true;
}

const char *tif_data_dirs(const char *xdg_data_dirs)
{
    return xdg_data_dirs && *xdg_data_dirs ? xdg_data_dirs : "/usr/local/share:/usr/share";
}

bool tif_distro_file_path(const char *data_dirs, char **out)
{
    const char *dir = data_dirs;
    *out = NULL;
    for (;;) {
        const char *end = strchr(dir, ':');
        size_t len = end ? (size_t)(end - dir) : strlen(dir);
        if (len > 0 && dir[0] == '/') {
            char *path;
            if (!concat(dir, len, BINDINGS_SUFFIX, &path))
                return false;
            if (access(path, F_OK) == 0) {
                *out = path;
                return true;
            }
            free(path);
        }
        if (!end)
            return true;
        dir = end + 1;
    }
}

bool tif_user_file_path(const char *xdg_config_home, const char *home, char **out)
{
    *out = NULL;
    if (xdg_config_home && xdg_config_home[0] == '/')
        return concat(xdg_config_home, strlen(xdg_config_home), BINDINGS_SUFFIX, out);
    if (home && *home)
        return concat(home, strlen(home), "/.config" BINDINGS_SUFFIX, out);
    return true;
}
