#ifndef TIF_WATCH_H
#define TIF_WATCH_H

#include <stdbool.h>
#include <stddef.h>

typedef struct tif_watch_target tif_watch_target;

typedef struct {
    int fd;
    tif_watch_target *targets;
    size_t count;
} tif_watch;

void tif_watch_init(tif_watch *w);
bool tif_watch_add_file(tif_watch *w, const char *path);
bool tif_watch_drain(tif_watch *w);
void tif_watch_free(tif_watch *w);

#endif
