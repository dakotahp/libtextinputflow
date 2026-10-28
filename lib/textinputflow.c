#include "textinputflow.h"

#include <stdlib.h>
#include <string.h>

#define KEY_BACKSPACE 0xff08u
#define KEY_HOME 0xff50u
#define KEY_LEFT 0xff51u
#define KEY_RIGHT 0xff53u
#define KEY_END 0xff57u
#define KEY_DELETE 0xffffu

#define SHIFT TIF_MOD_SHIFT
#define CTRL TIF_MOD_CONTROL
#define ALT TIF_MOD_ALT

#define BIND(sym, mods, action, unit, dir, extend) \
    { {(sym), (mods)}, false, {TIF_ACTION_##action, TIF_UNIT_##unit, TIF_DIRECTION_##dir, (extend)} }
#define BLOCK(sym, mods) \
    { {(sym), (mods)}, true, {(tif_action)0, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false} }

struct tif_ctx {
    tif_binding *bindings;
    size_t count;
};

static const tif_binding stub_table[] = {
    BIND(KEY_LEFT, 0, MOVE, CHARACTER, BACKWARD, false),
    BIND(KEY_RIGHT, 0, MOVE, CHARACTER, FORWARD, false),
    BIND(KEY_LEFT, SHIFT, MOVE, CHARACTER, BACKWARD, true),
    BIND(KEY_RIGHT, SHIFT, MOVE, CHARACTER, FORWARD, true),
    BIND(KEY_LEFT, CTRL, MOVE, WORD, BACKWARD, false),
    BLOCK(KEY_RIGHT, CTRL),
    BIND(KEY_LEFT, SHIFT | CTRL, MOVE, WORD, BACKWARD, true),
    BIND(KEY_RIGHT, SHIFT | CTRL, MOVE, WORD, FORWARD, true),
    BIND(KEY_HOME, 0, MOVE, LINE_ENDS, BACKWARD, false),
    BIND(KEY_END, 0, MOVE, LINE_ENDS, FORWARD, false),
    BIND(KEY_HOME, SHIFT, MOVE, LINE_ENDS, BACKWARD, true),
    BIND(KEY_END, SHIFT, MOVE, LINE_ENDS, FORWARD, true),
    BIND(KEY_HOME, CTRL, MOVE, DOCUMENT_ENDS, BACKWARD, false),
    BIND(KEY_END, CTRL, MOVE, DOCUMENT_ENDS, FORWARD, false),
    BIND(KEY_HOME, SHIFT | CTRL, MOVE, DOCUMENT_ENDS, BACKWARD, true),
    BIND(KEY_END, SHIFT | CTRL, MOVE, DOCUMENT_ENDS, FORWARD, true),
    BIND(KEY_BACKSPACE, 0, DELETE, CHARACTER, BACKWARD, false),
    BIND(KEY_DELETE, 0, DELETE, CHARACTER, FORWARD, false),
    BIND(KEY_BACKSPACE, CTRL, DELETE, WORD, BACKWARD, false),
    BIND(KEY_DELETE, CTRL, DELETE, WORD, FORWARD, false),
    BIND('a', CTRL, SELECT_ALL, NONE, NONE, false),
    BIND(KEY_LEFT, ALT, MOVE, WORD, BACKWARD, false),
    BIND(KEY_RIGHT, ALT, MOVE, WORD, FORWARD, false),
    BIND(KEY_RIGHT, SHIFT | ALT, MOVE, WORD, FORWARD, true),
    BIND(KEY_BACKSPACE, ALT, DELETE, WORD, BACKWARD, false),
};

#define STUB_COUNT (sizeof stub_table / sizeof stub_table[0])

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

tif_ctx *tif_new(void)
{
    tif_ctx *ctx = calloc(1, sizeof *ctx);
    if (!ctx)
        return NULL;
    ctx->bindings = malloc(sizeof stub_table);
    if (!ctx->bindings) {
        free(ctx);
        return NULL;
    }
    memcpy(ctx->bindings, stub_table, sizeof stub_table);
    ctx->count = STUB_COUNT;
    return ctx;
}

tif_ctx *tif_new_for_paths(const char *distro_file, const char *user_file)
{
    (void)distro_file;
    (void)user_file;
    return tif_new();
}

void tif_free(tif_ctx *ctx)
{
    if (!ctx)
        return;
    free(ctx->bindings);
    free(ctx);
}

bool tif_reload(tif_ctx *ctx)
{
    (void)ctx;
    return false;
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

    key = normalize(key);
    for (size_t i = 0; i < ctx->count; i++) {
        const tif_binding *b = &ctx->bindings[i];
        if (b->key.keysym != key.keysym || b->key.mods != key.mods)
            continue;
        if (b->blocked)
            return TIF_LOOKUP_BLOCKED;
        if (out)
            *out = b->command;
        return TIF_LOOKUP_COMMAND;
    }
    return TIF_LOOKUP_UNBOUND;
}

size_t tif_binding_count(const tif_ctx *ctx)
{
    return ctx ? ctx->count : 0;
}

bool tif_binding_at(const tif_ctx *ctx, size_t index, tif_binding *out)
{
    if (!ctx || !out || index >= ctx->count)
        return false;
    *out = ctx->bindings[index];
    return true;
}

size_t tif_keys_for_command(const tif_ctx *ctx, const tif_command *command, tif_key *out, size_t max)
{
    if (!ctx || !command || (!out && max > 0) || !valid_command(command))
        return 0;

    size_t total = 0;
    for (size_t i = 0; i < ctx->count; i++) {
        const tif_binding *b = &ctx->bindings[i];
        if (b->blocked || !same_command(&b->command, command))
            continue;
        if (total < max)
            out[total] = b->key;
        total++;
    }
    return total;
}
