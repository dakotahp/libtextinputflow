#include "util.h"

typedef struct {
    char *dir;
    char *path;
    tif_ctx *ctx;
    char *err;
} loaded;

static loaded load_text(const char *text)
{
    loaded l;
    l.dir = make_temp_dir();
    l.path = join(l.dir, "bindings");
    write_file(l.path, text);

    stderr_capture cap = capture_begin();
    l.ctx = tif_new_for_paths(l.path, NULL);
    l.err = capture_end(cap);
    if (!l.ctx)
        abort();
    return l;
}

static void unload(loaded *l)
{
    tif_free(l->ctx);
    free(l->err);
    free(l->path);
    remove_tree(l->dir);
}

static bool has_warning(const loaded *l, size_t line, const char *reason_part)
{
    char prefix[512];
    snprintf(prefix, sizeof prefix, "textinputflow: %s:%zu: ", l->path, line);
    size_t n = strlen(prefix);
    return strncmp(l->err, prefix, n) == 0 && strstr(l->err + n, reason_part) != NULL &&
           l->err[strlen(l->err) - 1] == '\n';
}

static const tif_command char_back = {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false};
static const tif_command char_fwd = {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false};

static void check_skipped(const char *name, const char *bad_line, const char *reason_part)
{
    size_t size = strlen(bad_line) + 128;
    char *text = malloc(size);
    if (!text)
        abort();
    snprintf(text, size, "Left = move character backward\n%s\nRight = move character forward\n", bad_line);

    int failures_before = check_failures;
    loaded l = load_text(text);
    CHECK(tif_binding_count(l.ctx) == 2);
    CHECK(looks_up_to(l.ctx, key(KEY_LEFT, 0), char_back));
    CHECK(looks_up_to(l.ctx, key(KEY_RIGHT, 0), char_fwd));
    CHECK(count_lines(l.err) == 1);
    CHECK(has_warning(&l, 2, reason_part));
    if (check_failures != failures_before)
        fprintf(stderr, "  case '%s': stderr was: %s\n", name, l.err);
    unload(&l);
    free(text);
}

static char *padded_line(size_t length)
{
    const char *head = "Left";
    const char *tail = "= move character backward";
    char *line = malloc(length + 1);
    if (!line)
        abort();
    size_t gap = length - strlen(head) - strlen(tail);
    memcpy(line, head, strlen(head));
    memset(line + strlen(head), ' ', gap);
    memcpy(line + strlen(head) + gap, tail, strlen(tail) + 1);
    return line;
}

static void check_error_lines(void)
{
    check_skipped("unknown key", "Control+Foo = move word backward", "unknown key");
    check_skipped("uppercase key", "Control+A = select-all", "uppercase");
    check_skipped("unknown modifier", "Hyper+Up = move document-start", "unknown modifier");
    check_skipped("repeated modifier", "Shift+shift+Up = move document-start", "repeated modifier");
    check_skipped("Ctrl and Control", "Ctrl+Control+Up = move document-start", "repeated modifier");
    check_skipped("missing =", "Control+Up move document-start", "missing '='");
    check_skipped("unknown command", "Control+Up = jump word backward", "unknown command");
    check_skipped("move without direction", "Control+Up = move word", "unknown command");
    check_skipped("extend on delete", "Control+BackSpace = delete word backward extend", "extend");
    check_skipped("extend on select-all", "Control+Up = select-all extend", "extend");
    check_skipped("trailing comment", "Control+Up = move document-start # top", "comment");
    check_skipped("non-ASCII byte", "Control+Up = move document-start \xc3\xa9", "ASCII");
    check_skipped("non-ASCII in comment", "# caf\xc3\xa9", "ASCII");
    check_skipped("control byte", "Control+Up = move\x01 document-start", "ASCII");
    check_skipped("space inside key", "Control + Up = move document-start", "key");
    check_skipped("missing key", "Control+ = move document-start", "key");
    check_skipped("empty key", " = move document-start", "key");
    check_skipped("missing command", "Control+Up =", "command");

    char *too_long = padded_line(1025);
    check_skipped("line over 1024 bytes", too_long, "1024");
    free(too_long);
}

static void check_longest_line_loads(void)
{
    char *line = padded_line(1024);
    size_t size = strlen(line) + 2;
    char *text = malloc(size);
    if (!text)
        abort();
    snprintf(text, size, "%s\n", line);

    loaded l = load_text(text);
    CHECK(tif_binding_count(l.ctx) == 1);
    CHECK(looks_up_to(l.ctx, key(KEY_LEFT, 0), char_back));
    CHECK(strcmp(l.err, "") == 0);
    unload(&l);
    free(text);
    free(line);
}

static void check_modifier_spelling(void)
{
    tif_command word_back_ext = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true);
    tif_command doc_start = command(TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, false);
    tif_command doc_end = command(TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, false);

    loaded l = load_text("ctrl+SHIFT+Left = move word backward extend\n"
                         "Ctrl+Up = move document-start\n"
                         "super+aLT+cOnTrOl+shift+Down = move document-end\n");
    CHECK(strcmp(l.err, "") == 0);
    CHECK(tif_binding_count(l.ctx) == 3);
    CHECK(looks_up_to(l.ctx, key(KEY_LEFT, S | C), word_back_ext));
    CHECK(looks_up_to(l.ctx, key(KEY_UP, C), doc_start));
    CHECK(looks_up_to(l.ctx, key(KEY_DOWN, S | C | A | W), doc_end));
    unload(&l);

    loaded same = load_text("Shift+Control+Left = move word backward\n"
                            "ctrl+SHIFT+Left = move word backward extend\n");
    CHECK(tif_binding_count(same.ctx) == 1);
    CHECK(looks_up_to(same.ctx, key(KEY_LEFT, S | C), word_back_ext));
    CHECK(count_lines(same.err) == 1);
    CHECK(has_warning(&same, 2, "line 1"));
    unload(&same);
}

static void check_every_key_name(void)
{
    loaded l = load_text("Left = none\nUp = none\nRight = none\nDown = none\n"
                         "Home = none\nEnd = none\nPage_Up = none\nPage_Down = none\n"
                         "BackSpace = none\nDelete = none\na = none\nz = none\nShift+q = none\n");
    static const uint32_t syms[] = {KEY_LEFT, KEY_UP,        KEY_RIGHT,     KEY_DOWN, KEY_HOME,
                                    KEY_END,  KEY_PAGE_UP,   KEY_PAGE_DOWN, KEY_BACKSPACE,
                                    KEY_DELETE, 'a',         'z'};
    CHECK(strcmp(l.err, "") == 0);
    CHECK(tif_binding_count(l.ctx) == 13);
    for (size_t i = 0; i < sizeof syms / sizeof syms[0]; i++)
        CHECK(tif_lookup(l.ctx, key(syms[i], 0), NULL) == TIF_LOOKUP_BLOCKED);
    CHECK(tif_lookup(l.ctx, key('Q', S), NULL) == TIF_LOOKUP_BLOCKED);
    unload(&l);
}

static void check_every_command(void)
{
    static const struct {
        const char *text;
        tif_command cmd;
    } cases[] = {
        {"move character backward", {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false}},
        {"move character forward extend", {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, true}},
        {"move word backward extend", {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true}},
        {"move word forward", {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false}},
        {"move line-start", {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, false}},
        {"move line-end extend", {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, true}},
        {"move document-start extend", {TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, true}},
        {"move document-end", {TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, false}},
        {"delete character backward", {TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false}},
        {"delete character forward", {TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false}},
        {"delete word backward", {TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false}},
        {"delete word forward", {TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false}},
        {"select-all", {TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false}},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char text[128];
        snprintf(text, sizeof text, "Alt+Up = %s\n", cases[i].text);
        int failures_before = check_failures;
        loaded l = load_text(text);
        CHECK(strcmp(l.err, "") == 0);
        CHECK(looks_up_to(l.ctx, key(KEY_UP, A), cases[i].cmd));
        if (check_failures != failures_before)
            fprintf(stderr, "  command '%s'\n", cases[i].text);
        unload(&l);
    }
}

static void check_blanks_and_comments(void)
{
    loaded l = load_text("# heading\n"
                         "\n"
                         "   \t  \n"
                         "  \t# indented comment\n"
                         "\t Left\t=\t \tmove   character\t backward  \n"
                         "Right=move character forward\r\n"
                         "Control+Right = none\n"
                         "Home = move line-start");
    tif_binding b;
    CHECK(strcmp(l.err, "") == 0);
    CHECK(tif_binding_count(l.ctx) == 4);
    CHECK(looks_up_to(l.ctx, key(KEY_LEFT, 0), char_back));
    CHECK(looks_up_to(l.ctx, key(KEY_RIGHT, 0), char_fwd));
    CHECK(looks_up_to(l.ctx, key(KEY_HOME, 0),
                      command(TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, false)));

    tif_command untouched = char_fwd;
    CHECK(tif_lookup(l.ctx, key(KEY_RIGHT, C), &untouched) == TIF_LOOKUP_BLOCKED);
    CHECK(same_command(untouched, char_fwd));
    CHECK(tif_binding_at(l.ctx, 2, &b));
    CHECK(b.blocked && b.key.keysym == KEY_RIGHT && b.key.mods == C);
    CHECK(b.command.action == 0 && b.command.unit == 0 && b.command.direction == 0 && !b.command.extend);
    unload(&l);
}

static void check_repeated_key(void)
{
    tif_command word_back = command(TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false);
    loaded l = load_text("Left = move character backward\n"
                         "Right = move character forward\n"
                         "Left = move word backward\n");
    CHECK(tif_binding_count(l.ctx) == 2);
    CHECK(looks_up_to(l.ctx, key(KEY_LEFT, 0), word_back));
    CHECK(count_lines(l.err) == 1);
    CHECK(has_warning(&l, 3, "line 1"));
    unload(&l);
}

static void check_empty_and_missing(void)
{
    loaded empty = load_text("");
    CHECK(tif_binding_count(empty.ctx) == 0);
    CHECK(strcmp(empty.err, "") == 0);
    unload(&empty);

    loaded comments = load_text("# nothing here\n\n");
    CHECK(tif_binding_count(comments.ctx) == 0);
    CHECK(strcmp(comments.err, "") == 0);
    unload(&comments);

    char *dir = make_temp_dir();
    char *missing = join(dir, "no-such-file");
    stderr_capture cap = capture_begin();
    tif_ctx *ctx = tif_new_for_paths(missing, missing);
    char *err = capture_end(cap);
    CHECK(ctx != NULL);
    CHECK(tif_binding_count(ctx) == 0);
    CHECK(strcmp(err, "") == 0);
    tif_free(ctx);
    free(err);
    free(missing);

    char *fifo = join(dir, "fifo");
    CHECK(mkfifo(fifo, 0600) == 0);
    cap = capture_begin();
    ctx = tif_new_for_paths(NULL, fifo);
    err = capture_end(cap);
    CHECK(ctx != NULL);
    CHECK(tif_binding_count(ctx) == 0);
    CHECK(strcmp(err, "") == 0);
    tif_free(ctx);
    free(err);
    free(fifo);

    cap = capture_begin();
    ctx = tif_new_for_paths(dir, NULL);
    err = capture_end(cap);
    CHECK(ctx != NULL);
    CHECK(tif_binding_count(ctx) == 0);
    CHECK(strcmp(err, "") == 0);
    tif_free(ctx);
    free(err);
    remove_tree(dir);
}

int main(void)
{
    check_error_lines();
    check_longest_line_loads();
    check_modifier_spelling();
    check_every_key_name();
    check_every_command();
    check_blanks_and_comments();
    check_repeated_key();
    check_empty_and_missing();
    return CHECK_DONE();
}
