#include "util.h"

static const char distro_text[] = "Left = move character backward\n"
                                  "Right = move character forward\n"
                                  "Control+Left = move word backward\n"
                                  "Control+a = select-all\n";

static const tif_command char_back = {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false};
static const tif_command char_fwd = {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false};
static const tif_command word_back = {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false};
static const tif_command select_all = {TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false};

static bool binding_is(const tif_ctx *ctx, size_t i, uint32_t keysym, uint32_t mods)
{
    tif_binding b;
    return tif_binding_at(ctx, i, &b) && b.key.keysym == keysym && b.key.mods == mods;
}

static void check_user_overrides(const char *distro, const char *user)
{
    write_file(distro, distro_text);
    write_file(user, "Left = move word backward\n"
                     "Control+Left = none\n"
                     "Alt+Left = move word backward\n"
                     "Alt+Right = none\n");

    tif_ctx *ctx = tif_new_for_paths(distro, user);
    CHECK(ctx != NULL);
    if (!ctx)
        return;

    CHECK(tif_binding_count(ctx) == 6);
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0), word_back));
    CHECK(looks_up_to(ctx, key(KEY_RIGHT, 0), char_fwd));
    CHECK(tif_lookup(ctx, key(KEY_LEFT, C), NULL) == TIF_LOOKUP_BLOCKED);
    CHECK(looks_up_to(ctx, key('a', C), select_all));
    CHECK(looks_up_to(ctx, key(KEY_LEFT, A), word_back));
    CHECK(tif_lookup(ctx, key(KEY_RIGHT, A), NULL) == TIF_LOOKUP_BLOCKED);

    CHECK(binding_is(ctx, 0, KEY_LEFT, 0));
    CHECK(binding_is(ctx, 1, KEY_RIGHT, 0));
    CHECK(binding_is(ctx, 2, KEY_LEFT, C));
    CHECK(binding_is(ctx, 3, 'a', C));
    CHECK(binding_is(ctx, 4, KEY_LEFT, A));
    CHECK(binding_is(ctx, 5, KEY_RIGHT, A));

    tif_key keys[4];
    memset(keys, 0, sizeof keys);
    CHECK(tif_keys_for_command(ctx, &word_back, keys, 4) == 2);
    CHECK(keys[0].keysym == KEY_LEFT && keys[0].mods == 0);
    CHECK(keys[1].keysym == KEY_LEFT && keys[1].mods == A);

    memset(keys, 0, sizeof keys);
    CHECK(tif_keys_for_command(ctx, &word_back, keys, 1) == 2);
    CHECK(keys[0].keysym == KEY_LEFT && keys[0].mods == 0);
    CHECK(keys[1].keysym == 0 && keys[1].mods == 0);
    CHECK(tif_keys_for_command(ctx, &word_back, NULL, 0) == 2);
    CHECK(tif_keys_for_command(ctx, &char_back, keys, 4) == 0);

    tif_command invalid = command(TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true);
    CHECK(tif_keys_for_command(ctx, &invalid, keys, 4) == 0);
    invalid = command(TIF_ACTION_MOVE, TIF_UNIT_NONE, TIF_DIRECTION_FORWARD, false);
    CHECK(tif_keys_for_command(ctx, &invalid, keys, 4) == 0);

    tif_free(ctx);
}

static void check_user_only(const char *user)
{
    write_file(user, "Control+a = select-all\n");
    tif_ctx *ctx = tif_new_for_paths(NULL, user);
    CHECK(ctx != NULL);
    CHECK(tif_binding_count(ctx) == 1);
    CHECK(looks_up_to(ctx, key('a', C), select_all));
    tif_free(ctx);
}

static void check_reload(const char *distro, const char *user)
{
    write_file(distro, distro_text);
    write_file(user, "Left = move word backward\n");

    tif_ctx *ctx = tif_new_for_paths(distro, user);
    CHECK(ctx != NULL);
    if (!ctx)
        return;
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0), word_back));

    CHECK(!tif_reload(ctx));

    write_file(user, "# a new comment\n\nLeft   =   move word backward\n");
    CHECK(!tif_reload(ctx));
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0), word_back));

    write_file(user, "Left = none\n");
    CHECK(tif_reload(ctx));
    CHECK(tif_lookup(ctx, key(KEY_LEFT, 0), NULL) == TIF_LOOKUP_BLOCKED);

    write_file(user, "Left = none\nLeft = move word backward extend\n");
    stderr_capture cap = capture_begin();
    CHECK(tif_reload(ctx));
    char *err = capture_end(cap);
    CHECK(count_lines(err) == 1);
    free(err);
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0), command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true)));

    remove(user);
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 4);
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0), char_back));

    CHECK(!tif_reload(ctx));

    write_file(distro, "Control+a = select-all\n");
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 1);
    CHECK(tif_lookup(ctx, key(KEY_LEFT, 0), NULL) == TIF_LOOKUP_UNBOUND);

    write_file(distro, "Control+a = select-all\nControl+Left = move word backward\n");
    CHECK(tif_reload(ctx));
    CHECK(tif_binding_count(ctx) == 2);

    write_file(distro, "Control+Left = move word backward\nControl+a = select-all\n");
    CHECK(tif_reload(ctx));
    CHECK(binding_is(ctx, 0, KEY_LEFT, C));

    tif_free(ctx);
}

int main(void)
{
    char *dir = make_temp_dir();
    char *distro = join(dir, "share/textinputflow/bindings");
    char *user = join(dir, "config/textinputflow/bindings");

    check_user_overrides(distro, user);
    check_user_only(user);
    check_reload(distro, user);

    free(distro);
    free(user);
    remove_tree(dir);
    return CHECK_DONE();
}
