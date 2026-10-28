#include "util.h"

#define FIXTURE_DIR "tests/fixtures/stub"
#define FIXTURE_FILE FIXTURE_DIR "/textinputflow/bindings"

typedef struct {
    uint32_t keysym;
    uint32_t mods;
    bool blocked;
    tif_action action;
    tif_unit unit;
    tif_direction direction;
    bool extend;
} expected_binding;

static const expected_binding expected[] = {
    {KEY_LEFT, 0, false, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false},
    {KEY_RIGHT, 0, false, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false},
    {KEY_LEFT, S, false, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, true},
    {KEY_RIGHT, S, false, TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, true},
    {KEY_LEFT, C, false, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false},
    {KEY_RIGHT, C, true, 0, 0, 0, false},
    {KEY_LEFT, S | C, false, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true},
    {KEY_RIGHT, S | C, false, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, true},
    {KEY_HOME, 0, false, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, false},
    {KEY_END, 0, false, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, false},
    {KEY_HOME, S, false, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, true},
    {KEY_END, S, false, TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, true},
    {KEY_HOME, C, false, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, false},
    {KEY_END, C, false, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, false},
    {KEY_HOME, S | C, false, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, true},
    {KEY_END, S | C, false, TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, true},
    {KEY_BACKSPACE, 0, false, TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false},
    {KEY_DELETE, 0, false, TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false},
    {KEY_BACKSPACE, C, false, TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false},
    {KEY_DELETE, C, false, TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false},
    {'a', C, false, TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false},
    {KEY_LEFT, A, false, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false},
    {KEY_RIGHT, A, false, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false},
    {KEY_RIGHT, S | A, false, TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, true},
    {KEY_BACKSPACE, A, false, TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false},
};

#define EXPECTED_COUNT (sizeof expected / sizeof expected[0])

static void check_stub_table(tif_ctx *ctx)
{
    tif_command out;

    CHECK(tif_binding_count(ctx) == 25);

    CHECK(looks_up_to(ctx, key(KEY_RIGHT, A), command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false)));
    CHECK(looks_up_to(ctx, key(KEY_LEFT, C), command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false)));
    CHECK(looks_up_to(ctx, key(KEY_RIGHT, S | A), command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, true)));

    tif_command untouched = command(TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false);
    out = untouched;
    CHECK(tif_lookup(ctx, key(KEY_RIGHT, C), &out) == TIF_LOOKUP_BLOCKED);
    CHECK(same_command(out, untouched));
    CHECK(tif_lookup(ctx, key(KEY_RIGHT, W), &out) == TIF_LOOKUP_UNBOUND);
    CHECK(same_command(out, untouched));
}

static void check_every_entry(tif_ctx *ctx)
{
    CHECK(EXPECTED_COUNT == 25);
    for (size_t i = 0; i < EXPECTED_COUNT; i++) {
        const expected_binding *e = &expected[i];
        tif_command out = {0, 0, 0, false};
        tif_binding b;
        int failures_before = check_failures;
        tif_lookup_result r = tif_lookup(ctx, key(e->keysym, e->mods), &out);
        CHECK(tif_binding_at(ctx, i, &b));
        CHECK(b.key.keysym == e->keysym && b.key.mods == e->mods && b.blocked == e->blocked);
        if (e->blocked) {
            CHECK(r == TIF_LOOKUP_BLOCKED);
            CHECK(b.command.action == 0 && b.command.unit == 0 && b.command.direction == 0 && !b.command.extend);
        } else {
            CHECK(r == TIF_LOOKUP_COMMAND);
            CHECK(same_command(out, command(e->action, e->unit, e->direction, e->extend)));
            CHECK(same_command(b.command, out));
        }
        if (check_failures != failures_before)
            fprintf(stderr, "  entry %zu: keysym 0x%x mods 0x%x\n", i, (unsigned)e->keysym, (unsigned)e->mods);
    }
}

static void check_reverse_lookup(tif_ctx *ctx)
{
    tif_key keys[4];
    tif_command word_forward = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false);
    tif_command word_backward = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false);

    memset(keys, 0, sizeof keys);
    CHECK(tif_keys_for_command(ctx, &word_forward, keys, 4) == 1);
    CHECK(keys[0].keysym == KEY_RIGHT && keys[0].mods == A);

    memset(keys, 0, sizeof keys);
    CHECK(tif_keys_for_command(ctx, &word_backward, keys, 4) == 2);
    CHECK(keys[0].keysym == KEY_LEFT && keys[0].mods == C);
    CHECK(keys[1].keysym == KEY_LEFT && keys[1].mods == A);
    CHECK(tif_keys_for_command(ctx, &word_backward, keys, 1) == 2);
    CHECK(tif_keys_for_command(ctx, &word_backward, NULL, 0) == 2);
}

static void check_context(tif_ctx *ctx, const char *name)
{
    int failures_before = check_failures;
    CHECK(ctx != NULL);
    if (ctx) {
        check_stub_table(ctx);
        check_every_entry(ctx);
        check_reverse_lookup(ctx);
        CHECK(tif_fd(ctx) == -1);
        CHECK(!tif_refresh(ctx));
        tif_free(ctx);
    }
    if (check_failures != failures_before)
        fprintf(stderr, "  context: %s\n", name);
}

int main(void)
{
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd))
        return 1;
    char *fixture_dir = join(cwd, FIXTURE_DIR);

    stderr_capture cap = capture_begin();
    tif_ctx *from_paths = tif_new_for_paths(NULL, FIXTURE_FILE);
    char *err = capture_end(cap);
    CHECK(strcmp(err, "") == 0);
    free(err);
    check_context(from_paths, "tif_new_for_paths(NULL, fixture)");

    setenv("XDG_CONFIG_HOME", fixture_dir, 1);
    setenv("XDG_DATA_DIRS", fixture_dir, 1);
    cap = capture_begin();
    tif_ctx *from_env = tif_new();
    err = capture_end(cap);
    CHECK(strcmp(err, "") == 0);
    free(err);
    check_context(from_env, "tif_new() with XDG variables set to the fixture folder");

    free(fixture_dir);
    return CHECK_DONE();
}
