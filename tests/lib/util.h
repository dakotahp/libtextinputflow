#ifndef TIF_TEST_UTIL_H
#define TIF_TEST_UTIL_H

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include "textinputflow.h"

#define KEY_BACKSPACE 0xff08u
#define KEY_HOME 0xff50u
#define KEY_LEFT 0xff51u
#define KEY_UP 0xff52u
#define KEY_RIGHT 0xff53u
#define KEY_DOWN 0xff54u
#define KEY_PAGE_UP 0xff55u
#define KEY_PAGE_DOWN 0xff56u
#define KEY_END 0xff57u
#define KEY_DELETE 0xffffu

#define S TIF_MOD_SHIFT
#define C TIF_MOD_CONTROL
#define A TIF_MOD_ALT
#define W TIF_MOD_SUPER

static inline tif_key key(uint32_t keysym, uint32_t mods)
{
    tif_key k = {keysym, mods};
    return k;
}

static inline tif_command command(tif_action action, tif_unit unit, tif_direction direction, bool extend)
{
    tif_command c = {action, unit, direction, extend};
    return c;
}

static inline bool same_command(tif_command a, tif_command b)
{
    return a.action == b.action && a.unit == b.unit && a.direction == b.direction && a.extend == b.extend;
}

static inline bool looks_up_to(const tif_ctx *ctx, tif_key k, tif_command expected)
{
    tif_command out = {0, 0, 0, false};
    return tif_lookup(ctx, k, &out) == TIF_LOOKUP_COMMAND && same_command(out, expected);
}

static inline char *join(const char *a, const char *b)
{
    size_t n = strlen(a) + strlen(b) + 2;
    char *s = malloc(n);
    if (!s)
        abort();
    snprintf(s, n, "%s/%s", a, b);
    return s;
}

static inline char *make_temp_dir(void)
{
    const char *base = getenv("TMPDIR");
    char *tmpl = join(base && *base ? base : "/tmp", "tif-test-XXXXXX");
    if (!mkdtemp(tmpl))
        abort();
    return tmpl;
}

static inline void make_dirs(const char *path)
{
    char *copy = malloc(strlen(path) + 1);
    if (!copy)
        abort();
    strcpy(copy, path);
    for (char *p = copy + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        mkdir(copy, 0700);
        *p = '/';
    }
    free(copy);
}

static inline void write_file(const char *path, const char *content)
{
    make_dirs(path);
    FILE *f = fopen(path, "wb");
    if (!f)
        abort();
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static inline int remove_entry(const char *path, const struct stat *st, int type, struct FTW *ftw)
{
    (void)st;
    (void)type;
    (void)ftw;
    return remove(path);
}

static inline void remove_tree(char *path)
{
    nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
    free(path);
}

typedef struct {
    int saved_fd;
    FILE *file;
} stderr_capture;

static inline stderr_capture capture_begin(void)
{
    stderr_capture c;
    const char *base = getenv("TMPDIR");
    char *path = join(base && *base ? base : "/tmp", "tif-stderr-XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0)
        abort();
    unlink(path);
    free(path);
    c.file = fdopen(fd, "w+");
    if (!c.file)
        abort();
    fflush(stderr);
    c.saved_fd = dup(STDERR_FILENO);
    dup2(fileno(c.file), STDERR_FILENO);
    return c;
}

static inline char *capture_end(stderr_capture c)
{
    fflush(stderr);
    dup2(c.saved_fd, STDERR_FILENO);
    close(c.saved_fd);

    fseek(c.file, 0, SEEK_END);
    long size = ftell(c.file);
    char *text = calloc(1, (size_t)size + 1);
    if (!text)
        abort();
    rewind(c.file);
    if (size > 0 && fread(text, 1, (size_t)size, c.file) != (size_t)size)
        abort();
    fclose(c.file);
    return text;
}

static inline size_t count_lines(const char *text)
{
    size_t n = 0;
    for (; *text; text++)
        n += *text == '\n';
    return n;
}

#endif
