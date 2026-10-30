#include <string.h>

#include "check.h"
#include "textinputflow.h"

int main(void)
{
    const char *version = tif_version();
    CHECK(version != NULL);
    CHECK(version && strcmp(version, TIF_EXPECTED_VERSION) == 0);
    CHECK(tif_version() == version);
    return CHECK_DONE();
}
