/* The library builds with hidden visibility; only the public header's functions are exported. */
#pragma GCC visibility push(default)
#include "textinputflow.h"
#pragma GCC visibility pop

#include <stdlib.h>
#include <string.h>

#include "paths.h"
#include "table.h"

struct tif_ctx {
    tif_table table;
    char *distro_path;
    char *user_path;
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

static bool load(const tif_ctx *ctx, tif_table *out)
{
    tif_table user;

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

static tif_ctx *create(char *distro_path, char *user_path)
{
    tif_ctx *ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
        free(distro_path);
        free(user_path);
        return NULL;
    }
    ctx->distro_path = distro_path;
    ctx->user_path = user_path;
    if (!load(ctx, &ctx->table)) {
        tif_free(ctx);
        return NULL;
    }
    return ctx;
}

tif_ctx *tif_new(void)
{
    return create(tif_distro_file_path(getenv("XDG_DATA_DIRS")),
                  tif_user_file_path(getenv("XDG_CONFIG_HOME"), getenv("HOME")));
}

tif_ctx *tif_new_for_paths(const char *distro_file, const char *user_file)
{
    char *distro_path;
    char *user_path;

    if (!copy_string(distro_file, &distro_path))
        return NULL;
    if (!copy_string(user_file, &user_path)) {
        free(distro_path);
        return NULL;
    }
    return create(distro_path, user_path);
}

void tif_free(tif_ctx *ctx)
{
    if (!ctx)
        return;
    tif_table_free(&ctx->table);
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
    (void)ctx;
    return -1;
}

bool tif_refresh(tif_ctx *ctx)
{
    (void)ctx;
    return false;
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
