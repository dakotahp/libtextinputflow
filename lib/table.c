#include "table.h"

#include <stdlib.h>

tif_entry *tif_table_find(const tif_table *table, tif_key key)
{
    for (size_t i = 0; i < table->count; i++) {
        tif_entry *e = &table->entries[i];
        if (e->binding.key.keysym == key.keysym && e->binding.key.mods == key.mods)
            return e;
    }
    return NULL;
}

bool tif_table_append(tif_table *table, const tif_entry *entry)
{
    if (table->count == table->capacity) {
        size_t capacity = table->capacity ? table->capacity * 2 : 32;
        tif_entry *entries = realloc(table->entries, capacity * sizeof *entries);
        if (!entries)
            return false;
        table->entries = entries;
        table->capacity = capacity;
    }
    table->entries[table->count++] = *entry;
    return true;
}

static bool same_binding(const tif_binding *a, const tif_binding *b)
{
    return a->key.keysym == b->key.keysym && a->key.mods == b->key.mods && a->blocked == b->blocked &&
           a->command.action == b->command.action && a->command.unit == b->command.unit &&
           a->command.direction == b->command.direction && a->command.extend == b->command.extend;
}

bool tif_table_equal(const tif_table *a, const tif_table *b)
{
    if (a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        if (!same_binding(&a->entries[i].binding, &b->entries[i].binding))
            return false;
    }
    return true;
}

void tif_table_free(tif_table *table)
{
    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}
