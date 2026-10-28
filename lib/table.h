#ifndef TIF_TABLE_H
#define TIF_TABLE_H

#include "textinputflow.h"

typedef struct {
    tif_binding binding;
    unsigned long line;
} tif_entry;

typedef struct {
    tif_entry *entries;
    size_t count;
    size_t capacity;
} tif_table;

tif_entry *tif_table_find(const tif_table *table, tif_key key);
bool tif_table_append(tif_table *table, const tif_entry *entry);
bool tif_table_equal(const tif_table *a, const tif_table *b);
void tif_table_free(tif_table *table);

bool tif_parse_file(const char *path, tif_table *out);

#endif
