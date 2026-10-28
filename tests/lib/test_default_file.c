#include "util.h"

static const char expected_text[] =
    "Left                = move character backward\n"
    "Right               = move character forward\n"
    "Shift+Left          = move character backward extend\n"
    "Shift+Right         = move character forward extend\n"
    "Control+Left        = move word backward\n"
    "Control+Right       = move word forward\n"
    "Shift+Control+Left  = move word backward extend\n"
    "Shift+Control+Right = move word forward extend\n"
    "Home                = move line-start\n"
    "End                 = move line-end\n"
    "Shift+Home          = move line-start extend\n"
    "Shift+End           = move line-end extend\n"
    "Control+Home        = move document-start\n"
    "Control+End         = move document-end\n"
    "Shift+Control+Home  = move document-start extend\n"
    "Shift+Control+End   = move document-end extend\n"
    "BackSpace           = delete character backward\n"
    "Delete              = delete character forward\n"
    "Control+BackSpace   = delete word backward\n"
    "Control+Delete      = delete word forward\n"
    "Control+a           = select-all\n";

typedef struct {
    uint32_t keysym;
    uint32_t mods;
    tif_action action;
    tif_unit unit;
    tif_direction direction;
    bool extend;
} expected_binding;

static const expected_binding expected[] = {
    {KEY_LEFT, 0, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false},
    {KEY_RIGHT, 0, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false},
    {KEY_LEFT, S, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, true},
    {KEY_RIGHT, S, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, true},
    {KEY_LEFT, C, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false},
    {KEY_RIGHT, C, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false},
    {KEY_LEFT, S | C, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true},
    {KEY_RIGHT, S | C, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, true},
    {KEY_HOME, 0, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, false},
    {KEY_END, 0, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, false},
    {KEY_HOME, S, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, true},
    {KEY_END, S, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, true},
    {KEY_HOME, C, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, false},
    {KEY_END, C, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, false},
    {KEY_HOME, S | C, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, true},
    {KEY_END, S | C, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, true},
    {KEY_BACKSPACE, 0, TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false},
    {KEY_DELETE, 0, TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false},
    {KEY_BACKSPACE, C, TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false},
    {KEY_DELETE, C, TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false},
    {'a', C, TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false},
};

#define EXPECTED_COUNT (sizeof expected / sizeof expected[0])

static void check_file_text(void)
{
    char text[sizeof expected_text + 64] = {0};
    FILE *f = fopen("data/bindings", "rb");
    CHECK(f != NULL);
    if (!f)
        return;
    size_t n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    CHECK(n == sizeof expected_text - 1);
    CHECK(strcmp(text, expected_text) == 0);
}

static void check_every_entry(const tif_ctx *ctx)
{
    CHECK(EXPECTED_COUNT == 21);
    CHECK(tif_binding_count(ctx) == EXPECTED_COUNT);

    for (size_t i = 0; i < EXPECTED_COUNT; i++) {
        const expected_binding *e = &expected[i];
        tif_command want = command(e->action, e->unit, e->direction, e->extend);
        tif_binding b;
        int failures_before = check_failures;

        CHECK(looks_up_to(ctx, key(e->keysym, e->mods), want));
        CHECK(tif_binding_at(ctx, i, &b));
        CHECK(!b.blocked && b.key.keysym == e->keysym && b.key.mods == e->mods && same_command(b.command, want));
        if (check_failures != failures_before)
            fprintf(stderr, "  entry %zu: keysym 0x%x mods 0x%x\n", i, (unsigned)e->keysym, (unsigned)e->mods);
    }
}

static void check_key_normalization(const tif_ctx *ctx)
{
    tif_command select_all = command(TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false);
    tif_command char_back = command(TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false);

    CHECK(looks_up_to(ctx, key(0x41, C), select_all));
    CHECK(looks_up_to(ctx, key('a', C | (1u << 4) | (1u << 13)), select_all));
    CHECK(looks_up_to(ctx, key(KEY_LEFT, 0xfffffff0u), char_back));
    CHECK(tif_lookup(ctx, key('a', 0), NULL) == TIF_LOOKUP_UNBOUND);
    CHECK(tif_lookup(ctx, key(KEY_LEFT, W), NULL) == TIF_LOOKUP_UNBOUND);
}

static void check_reverse_lookup(const tif_ctx *ctx)
{
    tif_key keys[4];
    tif_command word_back = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false);
    tif_command invalid = command(TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true);

    memset(keys, 0, sizeof keys);
    CHECK(tif_keys_for_command(ctx, &word_back, keys, 4) == 1);
    CHECK(keys[0].keysym == KEY_LEFT && keys[0].mods == C);
    CHECK(tif_keys_for_command(ctx, &invalid, keys, 4) == 0);
}

int main(void)
{
    check_file_text();

    stderr_capture cap = capture_begin();
    tif_ctx *ctx = tif_new_for_paths("data/bindings", NULL);
    char *output = capture_end(cap);
    CHECK(strcmp(output, "") == 0);
    free(output);

    CHECK(ctx != NULL);
    if (ctx) {
        check_every_entry(ctx);
        check_key_normalization(ctx);
        check_reverse_lookup(ctx);
        CHECK(tif_fd(ctx) == -1);
        CHECK(!tif_refresh(ctx));
        CHECK(!tif_reload(ctx));
        CHECK(tif_binding_count(ctx) == EXPECTED_COUNT);
        tif_free(ctx);
    }

    return CHECK_DONE();
}
