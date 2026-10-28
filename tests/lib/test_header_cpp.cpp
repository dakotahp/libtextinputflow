#include "textinputflow.h"

int main()
{
    tif_ctx *ctx = tif_new();
    const tif_key key = {0xff53u, TIF_MOD_ALT};
    tif_command out = {};
    const bool ok = ctx != nullptr && tif_lookup(ctx, key, &out) != TIF_LOOKUP_BLOCKED;
    tif_free(ctx);
    return ok ? 0 : 1;
}
