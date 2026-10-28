#ifndef TIF_PATHS_H
#define TIF_PATHS_H

const char *tif_data_dirs(const char *xdg_data_dirs);
char *tif_distro_file_path(const char *xdg_data_dirs);
char *tif_user_file_path(const char *xdg_config_home, const char *home);

#endif
