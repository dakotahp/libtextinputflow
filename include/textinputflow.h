#ifndef TEXTINPUTFLOW_H
#define TEXTINPUTFLOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIF_API_VERSION 0

typedef enum {
    TIF_ACTION_MOVE = 1,
    TIF_ACTION_DELETE = 2,
    TIF_ACTION_SELECT_ALL = 3
} tif_action;

typedef enum {
    TIF_UNIT_NONE = 0,
    TIF_UNIT_CHARACTER = 1,
    TIF_UNIT_WORD = 2,
    TIF_UNIT_LINE_ENDS = 3,
    TIF_UNIT_DOCUMENT_ENDS = 4
} tif_unit;

typedef enum {
    TIF_DIRECTION_BACKWARD = -1,
    TIF_DIRECTION_NONE = 0,
    TIF_DIRECTION_FORWARD = 1
} tif_direction;

enum {
    TIF_MOD_SHIFT = 1u << 0,
    TIF_MOD_CONTROL = 1u << 1,
    TIF_MOD_ALT = 1u << 2,
    TIF_MOD_SUPER = 1u << 3
};
#define TIF_MOD_MASK 0xFu

typedef struct {
    tif_action action;
    tif_unit unit;
    tif_direction direction;
    bool extend;
} tif_command;

typedef struct {
    uint32_t keysym;
    uint32_t mods;
} tif_key;

typedef struct {
    tif_key key;
    bool blocked;
    tif_command command;
} tif_binding;

typedef enum {
    TIF_LOOKUP_UNBOUND = 0,
    TIF_LOOKUP_COMMAND = 1,
    TIF_LOOKUP_BLOCKED = 2
} tif_lookup_result;

typedef struct tif_ctx tif_ctx;

tif_ctx *tif_new(void);
tif_ctx *tif_new_for_paths(const char *distro_file, const char *user_file);
void tif_free(tif_ctx *ctx);

bool tif_reload(tif_ctx *ctx);
int tif_fd(const tif_ctx *ctx);
bool tif_refresh(tif_ctx *ctx);

tif_lookup_result tif_lookup(const tif_ctx *ctx, tif_key key, tif_command *out);
size_t tif_binding_count(const tif_ctx *ctx);
bool tif_binding_at(const tif_ctx *ctx, size_t index, tif_binding *out);
size_t tif_keys_for_command(const tif_ctx *ctx, const tif_command *command,
                            tif_key *out, size_t max);

#ifdef __cplusplus
}
#endif

#endif
