#include "check.h"
#include "textinputflow.h"

int main(void)
{
    tif_key key = {0xff53u, TIF_MOD_ALT};
    tif_command cmd = {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false};
    tif_binding binding;
    tif_key keys[2];

    tif_free(NULL);
    CHECK(!tif_reload(NULL));
    CHECK(tif_fd(NULL) == -1);
    CHECK(!tif_refresh(NULL));
    CHECK(tif_lookup(NULL, key, &cmd) == TIF_LOOKUP_UNBOUND);
    CHECK(cmd.action == TIF_ACTION_MOVE && cmd.unit == TIF_UNIT_WORD && cmd.direction == TIF_DIRECTION_FORWARD &&
          !cmd.extend);
    CHECK(tif_binding_count(NULL) == 0);
    CHECK(!tif_binding_at(NULL, 0, &binding));
    CHECK(tif_keys_for_command(NULL, &cmd, keys, 2) == 0);

    tif_ctx *ctx = tif_new_for_paths(NULL, NULL);
    CHECK(ctx != NULL);
    CHECK(!tif_binding_at(ctx, 0, NULL));
    CHECK(tif_keys_for_command(ctx, NULL, keys, 2) == 0);
    CHECK(tif_keys_for_command(ctx, &cmd, NULL, 2) == 0);
    tif_free(ctx);

    return CHECK_DONE();
}
