#define _POSIX_C_SOURCE 200809L

#include "watch.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

/* No IN_MODIFY, so a write in place reloads once, on IN_CLOSE_WRITE, not on every write call. */
#define WATCH_MASK                                                                                   \
    (IN_ATTRIB | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO | \
     IN_ONLYDIR)

struct tif_watch_target {
    char *dir;
    char *file;
    char *parent;
    char *dir_name;
    int wd;
    int parent_wd;
};

static char *dir_of(const char *path, const char **name)
{
    const char *slash = strrchr(path, '/');
    if (!slash) {
        *name = path;
        return strdup(".");
    }
    *name = slash + 1;
    return slash == path ? strdup("/") : strndup(path, (size_t)(slash - path));
}

static bool fill_target(tif_watch_target *t, const char *path)
{
    const char *file;
    const char *dir_name;

    t->dir = dir_of(path, &file);
    if (!t->dir)
        return false;
    t->file = strdup(file);
    if (!t->file)
        return false;
    char *parent = dir_of(t->dir, &dir_name);
    if (!parent)
        return false;
    if (!*dir_name || strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0) {
        free(parent);
        return true;
    }
    t->parent = parent;
    t->dir_name = strdup(dir_name);
    return t->dir_name != NULL;
}

static void free_target(tif_watch_target *t)
{
    free(t->dir);
    free(t->file);
    free(t->parent);
    free(t->dir_name);
}

static void watch_target(int fd, tif_watch_target *t)
{
    if (t->wd < 0)
        t->wd = inotify_add_watch(fd, t->dir, WATCH_MASK);
    if (t->wd >= 0 || !t->parent || t->parent_wd >= 0)
        return;
    t->parent_wd = inotify_add_watch(fd, t->parent, WATCH_MASK);
    /* The folder may have been created after the first try but before the parent was watched. */
    if (t->parent_wd >= 0)
        t->wd = inotify_add_watch(fd, t->dir, WATCH_MASK);
}

static bool names(const struct inotify_event *e, const char *name)
{
    return e->len > 0 && strcmp(e->name, name) == 0;
}

static bool handle_event(tif_watch *w, const struct inotify_event *e)
{
    if (e->mask & IN_Q_OVERFLOW) {
        for (size_t i = 0; i < w->count; i++) {
            w->targets[i].wd = -1;
            w->targets[i].parent_wd = -1;
        }
        return true;
    }

    bool gone = (e->mask & (IN_IGNORED | IN_MOVE_SELF)) != 0;
    bool relevant = false;

    if (e->mask & IN_MOVE_SELF)
        inotify_rm_watch(w->fd, e->wd);
    for (size_t i = 0; i < w->count; i++) {
        tif_watch_target *t = &w->targets[i];
        if (e->wd == t->wd) {
            relevant = relevant || gone || names(e, t->file);
            if (gone)
                t->wd = -1;
        }
        if (e->wd == t->parent_wd) {
            relevant = relevant || names(e, t->dir_name);
            if (gone)
                t->parent_wd = -1;
        }
    }
    return relevant;
}

void tif_watch_init(tif_watch *w)
{
    w->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    w->targets = NULL;
    w->count = 0;
}

bool tif_watch_add_file(tif_watch *w, const char *path)
{
    if (w->fd < 0 || !path)
        return true;

    tif_watch_target *targets = realloc(w->targets, (w->count + 1) * sizeof *targets);
    if (!targets)
        return false;
    w->targets = targets;

    tif_watch_target t = {NULL, NULL, NULL, NULL, -1, -1};
    if (!fill_target(&t, path)) {
        free_target(&t);
        return false;
    }
    watch_target(w->fd, &t);
    w->targets[w->count++] = t;
    return true;
}

bool tif_watch_drain(tif_watch *w)
{
    union {
        struct inotify_event event;
        char bytes[4096];
    } buf;
    bool relevant = false;

    if (w->fd < 0)
        return false;
    for (;;) {
        ssize_t n = read(w->fd, buf.bytes, sizeof buf.bytes);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        for (ssize_t at = 0; at < n;) {
            const struct inotify_event *e = (const struct inotify_event *)(buf.bytes + at);
            relevant = handle_event(w, e) || relevant;
            at += (ssize_t)(sizeof *e + e->len);
        }
    }
    if (relevant) {
        for (size_t i = 0; i < w->count; i++)
            watch_target(w->fd, &w->targets[i]);
    }
    return relevant;
}

void tif_watch_free(tif_watch *w)
{
    for (size_t i = 0; i < w->count; i++)
        free_target(&w->targets[i]);
    free(w->targets);
    if (w->fd >= 0)
        close(w->fd);
}
