#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#include "util.h"

static const tif_command char_back = {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false};
static const tif_command char_fwd = {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false};
static const tif_command line_start = {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, false};
static const tif_command line_end = {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, false};

static bool readable_within(const tif_ctx *ctx, int timeout_ms)
{
    struct pollfd p = {tif_fd(ctx), POLLIN, 0};
    return p.fd >= 0 && poll(&p, 1, timeout_ms) == 1 && (p.revents & POLLIN);
}

static bool changes_after_event(tif_ctx *ctx)
{
    return readable_within(ctx, 1000) && tif_refresh(ctx);
}

static void check_fd_flags(const tif_ctx *ctx)
{
    int fd = tif_fd(ctx);
    CHECK(fd >= 0);
    CHECK((fcntl(fd, F_GETFL) & O_NONBLOCK) != 0);
    CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
}

static void check_idle_refresh(tif_ctx *ctx)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bool changed = tif_refresh(ctx);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    CHECK(!changed);
    CHECK(elapsed_ms < 1.0);
}

static void check_free_closes_fd(tif_ctx *ctx)
{
    int fd = tif_fd(ctx);
    tif_free(ctx);
    errno = 0;
    CHECK(fd >= 0 && fcntl(fd, F_GETFD) == -1 && errno == EBADF);
}

static void check_explicit_paths(const char *root)
{
    char *distro_dir = join(root, "distro");
    char *user_dir = join(root, "user");
    char *distro = join(distro_dir, "bindings");
    char *user = join(user_dir, "bindings");
    char *temp = join(user_dir, "bindings.tmp");
    char *notes = join(user_dir, "notes.txt");

    write_file(distro, "Left = move character backward\n");
    write_file(user, "Right = move character forward\n");

    tif_ctx *ctx = tif_new_for_paths(distro, user);
    if (!ctx)
        abort();
    check_fd_flags(ctx);
    check_idle_refresh(ctx);
    CHECK(!readable_within(ctx, 0));

    write_file(notes, "not bindings\n");
    CHECK(readable_within(ctx, 1000));
    CHECK(!tif_refresh(ctx));
    CHECK(!readable_within(ctx, 0));

    write_file(user, "Right = move character forward\nHome = move line-start\n");
    CHECK(changes_after_event(ctx));
    CHECK(looks_up_to(ctx, key(KEY_HOME, 0), line_start));

    write_file(temp, "Right = move character forward\nEnd = move line-end\n");
    CHECK(rename(temp, user) == 0);
    CHECK(changes_after_event(ctx));
    CHECK(looks_up_to(ctx, key(KEY_END, 0), line_end));
    CHECK(tif_lookup(ctx, key(KEY_HOME, 0), NULL) == TIF_LOOKUP_UNBOUND);

    CHECK(remove(user) == 0);
    CHECK(changes_after_event(ctx));
    CHECK(tif_lookup(ctx, key(KEY_RIGHT, 0), NULL) == TIF_LOOKUP_UNBOUND);
    CHECK(tif_lookup(ctx, key(KEY_END, 0), NULL) == TIF_LOOKUP_UNBOUND);
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0), char_back));
    CHECK(tif_binding_count(ctx) == 1);

    check_free_closes_fd(ctx);

    free(distro_dir);
    free(user_dir);
    free(distro);
    free(user);
    free(temp);
    free(notes);
}

static void check_folders_created_later(const char *root)
{
    char *config = join(root, "config");
    char *data_a = join(root, "data-a");
    char *data_b = join(root, "data-b");
    char *user = join(config, "textinputflow/bindings");
    char *distro_a = join(data_a, "textinputflow/bindings");
    char *distro_b = join(data_b, "textinputflow/bindings");
    char dirs[1024];

    CHECK(mkdir(config, 0700) == 0);
    CHECK(mkdir(data_a, 0700) == 0);
    write_file(distro_b, "Left = move character backward\n");

    setenv("HOME", root, 1);
    setenv("XDG_CONFIG_HOME", config, 1);
    snprintf(dirs, sizeof dirs, "%s:%s", data_a, data_b);
    setenv("XDG_DATA_DIRS", dirs, 1);

    tif_ctx *ctx = tif_new();
    if (!ctx)
        abort();
    check_fd_flags(ctx);
    CHECK(tif_binding_count(ctx) == 1);

    write_file(user, "Right = move character forward\n");
    CHECK(changes_after_event(ctx));
    CHECK(looks_up_to(ctx, key(KEY_RIGHT, 0), char_fwd));

    write_file(user, "Right = move character forward\nHome = move line-start\n");
    CHECK(changes_after_event(ctx));
    CHECK(looks_up_to(ctx, key(KEY_HOME, 0), line_start));

    write_file(distro_a, "Up = none\n");
    CHECK(changes_after_event(ctx));
    CHECK(tif_lookup(ctx, key(KEY_UP, 0), NULL) == TIF_LOOKUP_BLOCKED);
    CHECK(tif_lookup(ctx, key(KEY_LEFT, 0), NULL) == TIF_LOOKUP_UNBOUND);

    check_free_closes_fd(ctx);

    free(config);
    free(data_a);
    free(data_b);
    free(user);
    free(distro_a);
    free(distro_b);
}

static void check_missing_config_parent(const char *root)
{
    char *config = join(root, "no-such-config");
    setenv("XDG_CONFIG_HOME", config, 1);
    tif_ctx *ctx = tif_new();
    CHECK(ctx != NULL);
    tif_free(ctx);
    free(config);
}

int main(void)
{
    char *root = make_temp_dir();

    check_explicit_paths(root);
    check_folders_created_later(root);
    check_missing_config_parent(root);

    remove_tree(root);
    return CHECK_DONE();
}
