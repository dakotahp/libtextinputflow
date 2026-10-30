#include "textinputflow.h"

int main(void)
{
    tif_binding binding = {{0, 0}, false, {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false}};
    tif_ctx *ctx = tif_new();
    int ok = ctx != NULL && TIF_API_VERSION == 1 && (TIF_MOD_MASK & TIF_MOD_SUPER) && !binding.blocked;
    tif_free(ctx);
    return ok ? 0 : 1;
}
