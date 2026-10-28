#ifndef TIF_PATHS_H
#define TIF_PATHS_H

#include <stdbool.h>

const char *tif_data_dirs(const char *xdg_data_dirs);
bool tif_distro_file_path(const char *data_dirs, char **out);
bool tif_user_file_path(const char *xdg_config_home, const char *home, char **out);

#endif
