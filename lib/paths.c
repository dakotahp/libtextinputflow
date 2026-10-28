#define _POSIX_C_SOURCE 200809L

#include "paths.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BINDINGS_SUFFIX "/textinputflow/bindings"

static char *concat(const char *head, size_t head_len, const char *tail)
{
    size_t tail_len = strlen(tail);
    char *s = malloc(head_len + tail_len + 1);
    if (!s)
        return NULL;
    memcpy(s, head, head_len);
    memcpy(s + head_len, tail, tail_len + 1);
    return s;
}

const char *tif_data_dirs(const char *xdg_data_dirs)
{
    return xdg_data_dirs && *xdg_data_dirs ? xdg_data_dirs : "/usr/local/share:/usr/share";
}

char *tif_distro_file_path(const char *xdg_data_dirs)
{
    const char *dir = tif_data_dirs(xdg_data_dirs);
    for (;;) {
        const char *end = strchr(dir, ':');
        size_t len = end ? (size_t)(end - dir) : strlen(dir);
        if (len > 0) {
            char *path = concat(dir, len, BINDINGS_SUFFIX);
            if (!path || access(path, F_OK) == 0)
                return path;
            free(path);
        }
        if (!end)
            return NULL;
        dir = end + 1;
    }
}

char *tif_user_file_path(const char *xdg_config_home, const char *home)
{
    if (xdg_config_home && *xdg_config_home)
        return concat(xdg_config_home, strlen(xdg_config_home), BINDINGS_SUFFIX);
    if (home && *home)
        return concat(home, strlen(home), "/.config" BINDINGS_SUFFIX);
    return NULL;
}
