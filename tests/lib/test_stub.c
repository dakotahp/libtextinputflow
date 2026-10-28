#include <string.h>

#include "check.h"
#include "textinputflow.h"

#define KEY_LEFT 0xff51u
#define KEY_RIGHT 0xff53u
#define KEY_BACKSPACE 0xff08u

static tif_key key(uint32_t keysym, uint32_t mods)
{
    tif_key k = {keysym, mods};
    return k;
}

static tif_command command(tif_action action, tif_unit unit, tif_direction direction, bool extend)
{
    tif_command c = {action, unit, direction, extend};
    return c;
}

static bool same_command(tif_command a, tif_command b)
{
    return a.action == b.action && a.unit == b.unit && a.direction == b.direction && a.extend == b.extend;
}

static void check_stub_table(tif_ctx *ctx)
{
    tif_command out;

    CHECK(tif_binding_count(ctx) == 25);

    CHECK(tif_lookup(ctx, key(KEY_RIGHT, TIF_MOD_ALT), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false)));

    CHECK(tif_lookup(ctx, key(KEY_LEFT, TIF_MOD_CONTROL), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false)));

    CHECK(tif_lookup(ctx, key(KEY_RIGHT, TIF_MOD_SHIFT | TIF_MOD_ALT), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, true)));

    CHECK(tif_lookup(ctx, key(KEY_BACKSPACE, TIF_MOD_ALT), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, command(TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false)));

    CHECK(tif_lookup(ctx, key('a', TIF_MOD_CONTROL), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, command(TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false)));

    tif_command untouched = command(TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false);
    out = untouched;
    CHECK(tif_lookup(ctx, key(KEY_RIGHT, TIF_MOD_CONTROL), &out) == TIF_LOOKUP_BLOCKED);
    CHECK(same_command(out, untouched));

    CHECK(tif_lookup(ctx, key(KEY_RIGHT, TIF_MOD_SUPER), &out) == TIF_LOOKUP_UNBOUND);
    CHECK(same_command(out, untouched));

    CHECK(tif_lookup(ctx, key(KEY_RIGHT, TIF_MOD_ALT), NULL) == TIF_LOOKUP_COMMAND);
}

static void check_key_normalization(tif_ctx *ctx)
{
    tif_command out;
    tif_command select_all = command(TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false);

    CHECK(tif_lookup(ctx, key('A', TIF_MOD_CONTROL), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, select_all));

    CHECK(tif_lookup(ctx, key('a', TIF_MOD_CONTROL | (1u << 8)), &out) == TIF_LOOKUP_COMMAND);
    CHECK(same_command(out, select_all));
}

static void check_reverse_lookup(tif_ctx *ctx)
{
    tif_key keys[4];
    tif_command word_forward = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false);
    tif_command word_backward = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false);
    tif_command invalid = command(TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true);

    memset(keys, 0, sizeof keys);
    CHECK(tif_keys_for_command(ctx, &word_forward, keys, 4) == 1);
    CHECK(keys[0].keysym == KEY_RIGHT && keys[0].mods == TIF_MOD_ALT);

    CHECK(tif_keys_for_command(ctx, &word_backward, keys, 4) == 2);
    CHECK(tif_keys_for_command(ctx, &word_backward, keys, 1) == 2);
    CHECK(tif_keys_for_command(ctx, &word_backward, NULL, 0) == 2);

    CHECK(tif_keys_for_command(ctx, &invalid, keys, 4) == 0);
}

static void check_enumeration(tif_ctx *ctx)
{
    tif_binding b;
    size_t blocked = 0;
    size_t count = tif_binding_count(ctx);

    for (size_t i = 0; i < count; i++) {
        CHECK(tif_binding_at(ctx, i, &b));
        if (b.blocked) {
            blocked++;
            CHECK(b.key.keysym == KEY_RIGHT && b.key.mods == TIF_MOD_CONTROL);
            CHECK(b.command.action == 0 && b.command.unit == 0 && b.command.direction == 0 && !b.command.extend);
        }
    }
    CHECK(blocked == 1);
    CHECK(!tif_binding_at(ctx, count, &b));
}

static void check_no_watching(tif_ctx *ctx)
{
    CHECK(tif_fd(ctx) == -1);
    CHECK(!tif_refresh(ctx));
    CHECK(!tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 25);
}

int main(void)
{
    tif_ctx *ctx = tif_new();
    CHECK(ctx != NULL);
    if (ctx) {
        check_stub_table(ctx);
        check_key_normalization(ctx);
        check_reverse_lookup(ctx);
        check_enumeration(ctx);
        check_no_watching(ctx);
        tif_free(ctx);
    }

    tif_ctx *with_paths = tif_new_for_paths("/nonexistent/distro", "/nonexistent/user");
    CHECK(with_paths != NULL);
    if (with_paths) {
        check_stub_table(with_paths);
        tif_free(with_paths);
    }

    return CHECK_DONE();
}
