/* The library builds with hidden visibility; only the public header's functions are exported. */
#pragma GCC visibility push(default)
#include "textinputflow.h"
#pragma GCC visibility pop

#include <stdlib.h>
#include <string.h>

#include "paths.h"
#include "table.h"
#include "watch.h"

struct tif_ctx {
    tif_table table;
    char *data_dirs;
    char *distro_path;
    char *user_path;
    tif_watch watch;
};

static tif_key normalize(tif_key key)
{
    key.mods &= TIF_MOD_MASK;
    if (key.keysym >= 'A' && key.keysym <= 'Z')
        key.keysym += 'a' - 'A';
    return key;
}

static bool valid_command(const tif_command *c)
{
    bool has_direction = c->direction == TIF_DIRECTION_BACKWARD || c->direction == TIF_DIRECTION_FORWARD;

    switch (c->action) {
    case TIF_ACTION_MOVE:
        return has_direction && c->unit >= TIF_UNIT_CHARACTER && c->unit <= TIF_UNIT_DOCUMENT_ENDS;
    case TIF_ACTION_DELETE:
        return has_direction && !c->extend && (c->unit == TIF_UNIT_CHARACTER || c->unit == TIF_UNIT_WORD);
    case TIF_ACTION_SELECT_ALL:
        return c->unit == TIF_UNIT_NONE && c->direction == TIF_DIRECTION_NONE && !c->extend;
    }
    return false;
}

static bool same_command(const tif_command *a, const tif_command *b)
{
    return a->action == b->action && a->unit == b->unit && a->direction == b->direction &&
           a->extend == b->extend;
}

static bool copy_string(const char *s, char **out)
{
    *out = NULL;
    if (!s)
        return true;
    size_t n = strlen(s) + 1;
    *out = malloc(n);
    if (!*out)
        return false;
    memcpy(*out, s, n);
    return true;
}

static bool load(tif_ctx *ctx, tif_table *out)
{
    tif_table user;

    if (ctx->data_dirs) {
        char *found;
        if (!tif_distro_file_path(ctx->data_dirs, &found))
            return false;
        free(ctx->distro_path);
        ctx->distro_path = found;
    }
    if (!tif_parse_file(ctx->distro_path, out))
        return false;
    if (!tif_parse_file(ctx->user_path, &user)) {
        tif_table_free(out);
        return false;
    }

    for (size_t i = 0; i < user.count; i++) {
        tif_entry *existing = tif_table_find(out, user.entries[i].binding.key);
        if (existing) {
            *existing = user.entries[i];
        } else if (!tif_table_append(out, &user.entries[i])) {
            tif_table_free(&user);
            tif_table_free(out);
            return false;
        }
    }
    tif_table_free(&user);
    return true;
}

static tif_ctx *new_ctx(void)
{
    tif_ctx *ctx = calloc(1, sizeof *ctx);
    if (ctx)
        tif_watch_init(&ctx->watch);
    return ctx;
}

static bool watch_distro_paths(tif_ctx *ctx)
{
    const char *cursor = ctx->data_dirs;
    for (;;) {
        char *path;
        if (!tif_next_distro_path(&cursor, &path))
            return false;
        if (!path)
            return true;
        bool ok = tif_watch_add_file(&ctx->watch, path);
        free(path);
        if (!ok)
            return false;
    }
}

static tif_ctx *finish(tif_ctx *ctx, bool setup_ok)
{
    if (!setup_ok || !load(ctx, &ctx->table)) {
        tif_free(ctx);
        return NULL;
    }
    return ctx;
}

tif_ctx *tif_new(void)
{
    tif_ctx *ctx = new_ctx();
    if (!ctx)
        return NULL;
    bool ok = copy_string(tif_data_dirs(getenv("XDG_DATA_DIRS")), &ctx->data_dirs) &&
              tif_user_file_path(getenv("XDG_CONFIG_HOME"), getenv("HOME"), &ctx->user_path) &&
              watch_distro_paths(ctx) && tif_watch_add_file(&ctx->watch, ctx->user_path);
    return finish(ctx, ok);
}

tif_ctx *tif_new_for_paths(const char *distro_file, const char *user_file)
{
    tif_ctx *ctx = new_ctx();
    if (!ctx)
        return NULL;
    bool ok = copy_string(distro_file, &ctx->distro_path) && copy_string(user_file, &ctx->user_path) &&
              tif_watch_add_file(&ctx->watch, distro_file) && tif_watch_add_file(&ctx->watch, user_file);
    return finish(ctx, ok);
}

void tif_free(tif_ctx *ctx)
{
    if (!ctx)
        return;
    tif_table_free(&ctx->table);
    tif_watch_free(&ctx->watch);
    free(ctx->data_dirs);
    free(ctx->distro_path);
    free(ctx->user_path);
    free(ctx);
}

bool tif_reload(tif_ctx *ctx)
{
    tif_table fresh;

    if (!ctx || !load(ctx, &fresh))
        return false;
    bool changed = !tif_table_equal(&ctx->table, &fresh);
    tif_table_free(&ctx->table);
    ctx->table = fresh;
    return changed;
}

int tif_fd(const tif_ctx *ctx)
{
    return ctx ? ctx->watch.fd : -1;
}

bool tif_refresh(tif_ctx *ctx)
{
    return ctx && tif_watch_drain(&ctx->watch) && tif_reload(ctx);
}

tif_lookup_result tif_lookup(const tif_ctx *ctx, tif_key key, tif_command *out)
{
    if (!ctx)
        return TIF_LOOKUP_UNBOUND;

    const tif_entry *e = tif_table_find(&ctx->table, normalize(key));
    if (!e)
        return TIF_LOOKUP_UNBOUND;
    if (e->binding.blocked)
        return TIF_LOOKUP_BLOCKED;
    if (out)
        *out = e->binding.command;
    return TIF_LOOKUP_COMMAND;
}

size_t tif_binding_count(const tif_ctx *ctx)
{
    return ctx ? ctx->table.count : 0;
}

bool tif_binding_at(const tif_ctx *ctx, size_t index, tif_binding *out)
{
    if (!ctx || !out || index >= ctx->table.count)
        return false;
    *out = ctx->table.entries[index].binding;
    return true;
}

size_t tif_keys_for_command(const tif_ctx *ctx, const tif_command *command, tif_key *out, size_t max)
{
    if (!ctx || !command || (!out && max > 0) || !valid_command(command))
        return 0;

    size_t total = 0;
    for (size_t i = 0; i < ctx->table.count; i++) {
        const tif_binding *b = &ctx->table.entries[i].binding;
        if (b->blocked || !same_command(&b->command, command))
            continue;
        if (total < max)
            out[total] = b->key;
        total++;
    }
    return total;
}
