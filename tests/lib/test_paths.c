#include "paths.h"
#include "util.h"

static char *user_path(const char *config_home, const char *home)
{
    char *path;
    if (!tif_user_file_path(config_home, home, &path))
        abort();
    return path;
}

static char *distro_path(const char *data_dirs)
{
    char *path;
    if (!tif_distro_file_path(data_dirs, &path))
        abort();
    return path;
}

static bool str_is(char *got, const char *want)
{
    bool same = (got == NULL && want == NULL) || (got && want && strcmp(got, want) == 0);
    if (!same)
        fprintf(stderr, "  got '%s', want '%s'\n", got ? got : "(null)", want ? want : "(null)");
    free(got);
    return same;
}

static void check_data_dirs_default(void)
{
    CHECK(strcmp(tif_data_dirs(NULL), "/usr/local/share:/usr/share") == 0);
    CHECK(strcmp(tif_data_dirs(""), "/usr/local/share:/usr/share") == 0);
    CHECK(strcmp(tif_data_dirs("/opt/share"), "/opt/share") == 0);
}

static void check_user_path(void)
{
    CHECK(str_is(user_path("/cfg", "/home/u"), "/cfg/textinputflow/bindings"));
    CHECK(str_is(user_path(NULL, "/home/u"), "/home/u/.config/textinputflow/bindings"));
    CHECK(str_is(user_path("", "/home/u"), "/home/u/.config/textinputflow/bindings"));
    CHECK(str_is(user_path(NULL, NULL), NULL));
    CHECK(str_is(user_path("", ""), NULL));
    CHECK(str_is(user_path("relative/cfg", "/home/u"), "/home/u/.config/textinputflow/bindings"));
}

static void check_distro_path(const char *root)
{
    char *dir_a = join(root, "a");
    char *dir_b = join(root, "b");
    char *file_a = join(dir_a, "textinputflow/bindings");
    char *file_b = join(dir_b, "textinputflow/bindings");
    char dirs[1024];

    make_dirs(file_a);
    write_file(file_b, "Left = move character backward\n");

    snprintf(dirs, sizeof dirs, "%s:%s", dir_a, dir_b);
    CHECK(str_is(distro_path(dirs), file_b));

    snprintf(dirs, sizeof dirs, "::%s:", dir_b);
    CHECK(str_is(distro_path(dirs), file_b));

    write_file(file_a, "Right = move character forward\n");
    snprintf(dirs, sizeof dirs, "%s:%s", dir_a, dir_b);
    CHECK(str_is(distro_path(dirs), file_a));

    snprintf(dirs, sizeof dirs, "%s/none:%s/also-none", root, root);
    CHECK(str_is(distro_path(dirs), NULL));

    CHECK(chdir(root) == 0);
    snprintf(dirs, sizeof dirs, "a:%s", dir_b);
    CHECK(str_is(distro_path(dirs), file_b));
    CHECK(str_is(distro_path("a"), NULL));

    free(dir_a);
    free(dir_b);
    free(file_a);
    free(file_b);
}

static size_t loaded_count(void)
{
    tif_ctx *ctx = tif_new();
    size_t n = tif_binding_count(ctx);
    tif_free(ctx);
    return n;
}

static void check_tif_new_uses_environment(const char *root)
{
    char *dir_a = join(root, "env-a");
    char *dir_b = join(root, "env-b");
    char *home = join(root, "home");
    char *config = join(root, "config");
    char *file_a = join(dir_a, "textinputflow/bindings");
    char *file_b = join(dir_b, "textinputflow/bindings");
    char *home_user = join(home, ".config/textinputflow/bindings");
    char *config_user = join(config, "textinputflow/bindings");
    char dirs[1024];

    write_file(file_a, "Left = move character backward\n");
    write_file(file_b, "Left = move character backward\nRight = move character forward\n");
    write_file(home_user, "Home = move line-start\n");
    write_file(config_user, "Home = move line-start\nEnd = move line-end\nUp = none\n");

    setenv("HOME", home, 1);
    unsetenv("XDG_CONFIG_HOME");
    snprintf(dirs, sizeof dirs, "%s:%s", dir_a, dir_b);
    setenv("XDG_DATA_DIRS", dirs, 1);
    CHECK(loaded_count() == 2);

    snprintf(dirs, sizeof dirs, "%s/missing:%s", root, dir_b);
    setenv("XDG_DATA_DIRS", dirs, 1);
    CHECK(loaded_count() == 3);

    setenv("XDG_CONFIG_HOME", "", 1);
    CHECK(loaded_count() == 3);

    setenv("XDG_CONFIG_HOME", config, 1);
    CHECK(loaded_count() == 5);

    tif_ctx *ctx = tif_new();
    CHECK(looks_up_to(ctx, key(KEY_RIGHT, 0),
                      command(TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false)));
    CHECK(tif_lookup(ctx, key(KEY_UP, 0), NULL) == TIF_LOOKUP_BLOCKED);
    remove(config_user);
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 2);
    tif_free(ctx);

    free(dir_a);
    free(dir_b);
    free(home);
    free(config);
    free(file_a);
    free(file_b);
    free(home_user);
    free(config_user);
}

static void check_reload_finds_distro_again(const char *root)
{
    char *dir_a = join(root, "reload-a");
    char *dir_b = join(root, "reload-b");
    char *file_a = join(dir_a, "textinputflow/bindings");
    char *file_b = join(dir_b, "textinputflow/bindings");
    char dirs[1024];

    setenv("HOME", root, 1);
    unsetenv("XDG_CONFIG_HOME");
    snprintf(dirs, sizeof dirs, "%s:%s", dir_a, dir_b);
    setenv("XDG_DATA_DIRS", dirs, 1);

    tif_ctx *ctx = tif_new();
    CHECK(tif_binding_count(ctx) == 0);

    write_file(file_b, "Left = move character backward\nRight = move character forward\n");
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 2);

    write_file(file_a, "Left = move character backward\n");
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 1);

    remove(file_a);
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 2);
    tif_free(ctx);

    free(dir_a);
    free(dir_b);
    free(file_a);
    free(file_b);
}

int main(void)
{
    char *root = make_temp_dir();

    check_data_dirs_default();
    check_user_path();
    check_distro_path(root);
    check_tif_new_uses_environment(root);
    check_reload_finds_distro_again(root);

    remove_tree(root);
    return CHECK_DONE();
}
