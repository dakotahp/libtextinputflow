#include "table.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_WORDS 5
#define REASON_SIZE (MAX_LINE + 128)

typedef enum { LINE_EMPTY, LINE_BINDING, LINE_ERROR } line_kind;

typedef enum { READ_OK, READ_NONE, READ_OUT_OF_MEMORY } read_status;

static const struct {
    const char *name;
    uint32_t mod;
} modifiers[] = {
    {"shift", TIF_MOD_SHIFT}, {"control", TIF_MOD_CONTROL}, {"ctrl", TIF_MOD_CONTROL},
    {"alt", TIF_MOD_ALT},     {"super", TIF_MOD_SUPER},
};

static const struct {
    const char *name;
    uint32_t keysym;
} keys[] = {
    {"Left", 0xff51u},     {"Up", 0xff52u},  {"Right", 0xff53u},   {"Down", 0xff54u},
    {"Home", 0xff50u},     {"End", 0xff57u}, {"Page_Up", 0xff55u}, {"Page_Down", 0xff56u},
    {"BackSpace", 0xff08u}, {"Delete", 0xffffu},
};

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

static bool fail(char *reason, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(reason, REASON_SIZE, format, args);
    va_end(args);
    return false;
}

static bool is_blank(char c)
{
    return c == ' ' || c == '\t';
}

static char *skip_blanks(char *s)
{
    while (is_blank(*s))
        s++;
    return s;
}

static void trim_end(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && is_blank(s[len - 1]))
        s[--len] = '\0';
}

static bool equal_ignoring_case(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
    }
    return *a == *b;
}

static uint32_t find_modifier(const char *name)
{
    for (size_t i = 0; i < COUNT(modifiers); i++) {
        if (equal_ignoring_case(name, modifiers[i].name))
            return modifiers[i].mod;
    }
    return 0;
}

static bool parse_key_name(const char *name, uint32_t *keysym, char *reason)
{
    if (name[0] >= 'a' && name[0] <= 'z' && name[1] == '\0') {
        *keysym = (uint32_t)name[0];
        return true;
    }
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
        return fail(reason, "uppercase letter key '%s': write '%c', with Shift if needed", name, tolower(name[0]));
    for (size_t i = 0; i < COUNT(keys); i++) {
        if (strcmp(name, keys[i].name) == 0) {
            *keysym = keys[i].keysym;
            return true;
        }
    }
    return fail(reason, "unknown key name '%s'", name);
}

static bool parse_keyspec(char *spec, tif_key *out, char *reason)
{
    uint32_t mods = 0;
    char *token = spec;
    char *plus;

    while ((plus = strchr(token, '+')) != NULL) {
        *plus = '\0';
        if (*token == '\0')
            return fail(reason, "empty modifier before '+'");
        uint32_t mod = find_modifier(token);
        if (!mod)
            return fail(reason, "unknown modifier '%s'", token);
        if (mods & mod)
            return fail(reason, "repeated modifier '%s'", token);
        mods |= mod;
        token = plus + 1;
    }
    if (*token == '\0')
        return fail(reason, "missing key after '+'");

    out->mods = mods;
    return parse_key_name(token, &out->keysym, reason);
}

static bool parse_direction(const char *word, tif_direction *out)
{
    if (strcmp(word, "backward") == 0)
        *out = TIF_DIRECTION_BACKWARD;
    else if (strcmp(word, "forward") == 0)
        *out = TIF_DIRECTION_FORWARD;
    else
        return false;
    return true;
}

static bool parse_unit(const char *word, tif_unit *out)
{
    if (strcmp(word, "character") == 0)
        *out = TIF_UNIT_CHARACTER;
    else if (strcmp(word, "word") == 0)
        *out = TIF_UNIT_WORD;
    else
        return false;
    return true;
}

static bool parse_ends(const char *word, tif_command *c)
{
    static const struct {
        const char *name;
        tif_unit unit;
        tif_direction direction;
    } ends[] = {
        {"line-start", TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD},
        {"line-end", TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD},
        {"document-start", TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD},
        {"document-end", TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD},
    };
    for (size_t i = 0; i < COUNT(ends); i++) {
        if (strcmp(word, ends[i].name) == 0) {
            c->unit = ends[i].unit;
            c->direction = ends[i].direction;
            return true;
        }
    }
    return false;
}

static size_t split_words(char *text, char **words)
{
    size_t n = 0;
    char *p = text;
    while (*(p = skip_blanks(p))) {
        if (n == MAX_WORDS + 1)
            return n;
        words[n++] = p;
        while (*p && !is_blank(*p))
            p++;
        if (*p)
            *p++ = '\0';
    }
    return n;
}

static bool parse_command(const char *text, tif_binding *out, char *reason)
{
    char buf[MAX_LINE + 1];
    char *w[MAX_WORDS + 1];
    tif_command c = {(tif_action)0, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false};
    bool blocked = false;
    bool known = false;

    strcpy(buf, text);
    size_t n = split_words(buf, w);
    bool extend = n > 1 && n <= MAX_WORDS && strcmp(w[n - 1], "extend") == 0;
    if (extend)
        n--;

    if (n == 1 && strcmp(w[0], "none") == 0) {
        blocked = true;
        known = true;
    } else if (n == 1 && strcmp(w[0], "select-all") == 0) {
        c.action = TIF_ACTION_SELECT_ALL;
        known = true;
    } else if (n == 2 && strcmp(w[0], "move") == 0) {
        c.action = TIF_ACTION_MOVE;
        known = parse_ends(w[1], &c);
    } else if (n == 3 && (strcmp(w[0], "move") == 0 || strcmp(w[0], "delete") == 0)) {
        c.action = strcmp(w[0], "move") == 0 ? TIF_ACTION_MOVE : TIF_ACTION_DELETE;
        known = parse_unit(w[1], &c.unit) && parse_direction(w[2], &c.direction);
    }

    if (!known)
        return fail(reason, "unknown command '%s'", text);
    if (extend && (blocked || c.action != TIF_ACTION_MOVE))
        return fail(reason, "'extend' is only allowed on move commands");

    c.extend = extend;
    out->blocked = blocked;
    out->command = c;
    return true;
}

static line_kind parse_line(const char *line, size_t len, tif_binding *out, char *reason)
{
    char buf[MAX_LINE + 1];

    if (len > MAX_LINE) {
        fail(reason, "line longer than %d bytes", MAX_LINE);
        return LINE_ERROR;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)line[i];
        if (c != '\t' && (c < 0x20 || c > 0x7e)) {
            fail(reason, "byte 0x%02x is outside printable ASCII", c);
            return LINE_ERROR;
        }
    }
    memcpy(buf, line, len);
    buf[len] = '\0';

    char *p = skip_blanks(buf);
    if (*p == '\0' || *p == '#')
        return LINE_EMPTY;
    if (strchr(p, '#')) {
        fail(reason, "a '#' comment must be on its own line");
        return LINE_ERROR;
    }

    char *eq = strchr(p, '=');
    if (!eq) {
        fail(reason, "missing '='");
        return LINE_ERROR;
    }
    *eq = '\0';
    trim_end(p);
    if (*p == '\0') {
        fail(reason, "missing key before '='");
        return LINE_ERROR;
    }
    if (strpbrk(p, " \t")) {
        fail(reason, "spaces inside the key '%s'", p);
        return LINE_ERROR;
    }
    if (!parse_keyspec(p, &out->key, reason))
        return LINE_ERROR;

    char *command = skip_blanks(eq + 1);
    trim_end(command);
    if (*command == '\0') {
        fail(reason, "missing command after '='");
        return LINE_ERROR;
    }
    return parse_command(command, out, reason) ? LINE_BINDING : LINE_ERROR;
}

static read_status read_file(const char *path, char **data, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return READ_NONE;

    char *buf = NULL;
    size_t len = 0;
    size_t capacity = 0;
    for (;;) {
        if (len == capacity) {
            capacity = capacity ? capacity * 2 : 4096;
            char *bigger = realloc(buf, capacity);
            if (!bigger) {
                free(buf);
                fclose(f);
                return READ_OUT_OF_MEMORY;
            }
            buf = bigger;
        }
        size_t n = fread(buf + len, 1, capacity - len, f);
        if (n == 0)
            break;
        len += n;
    }

    bool failed = ferror(f) != 0;
    fclose(f);
    if (failed) {
        free(buf);
        return READ_NONE;
    }
    *data = buf;
    *size = len;
    return READ_OK;
}

static void warn(const char *path, unsigned long line, const char *reason)
{
    fprintf(stderr, "textinputflow: %s:%lu: %s\n", path, line, reason);
}

bool tif_parse_file(const char *path, tif_table *out)
{
    char *data;
    size_t size;

    *out = (tif_table){NULL, 0, 0};
    if (!path)
        return true;
    read_status status = read_file(path, &data, &size);
    if (status != READ_OK)
        return status == READ_NONE;

    unsigned long line_number = 0;
    size_t pos = 0;
    while (pos < size) {
        const char *line = data + pos;
        const char *newline = memchr(line, '\n', size - pos);
        size_t len = newline ? (size_t)(newline - line) : size - pos;
        pos += len + (newline ? 1 : 0);
        line_number++;
        if (len > 0 && line[len - 1] == '\r')
            len--;

        char reason[REASON_SIZE];
        tif_entry entry = {{{0, 0}, false, {(tif_action)0, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false}}, line_number};
        line_kind kind = parse_line(line, len, &entry.binding, reason);
        if (kind == LINE_ERROR) {
            warn(path, line_number, reason);
            continue;
        }
        if (kind == LINE_EMPTY)
            continue;

        tif_entry *earlier = tif_table_find(out, entry.binding.key);
        if (earlier) {
            snprintf(reason, sizeof reason, "key already bound on line %lu; this line wins", earlier->line);
            warn(path, line_number, reason);
            *earlier = entry;
        } else if (!tif_table_append(out, &entry)) {
            free(data);
            tif_table_free(out);
            return false;
        }
    }

    free(data);
    return true;
}
