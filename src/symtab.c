#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

#define HASH_SIZE 1024

static Symbol *hash_table[HASH_SIZE];

static unsigned int hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % HASH_SIZE;
}

void symtab_init(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        hash_table[i] = NULL;
    }
}

void symtab_insert(const char *name, const char *domain) {
    if (symtab_lookup(name) != NULL) {
        return; // Already exists
    }

    Symbol *s = malloc(sizeof(Symbol));
    strncpy(s->name, name, MAX_NAME_LEN - 1);
    s->name[MAX_NAME_LEN - 1] = '\0';
    
    strncpy(s->domain, domain, MAX_DOMAIN_LEN - 1);
    s->domain[MAX_DOMAIN_LEN - 1] = '\0';

    unsigned int h = hash(name);
    s->next = hash_table[h];
    hash_table[h] = s;
}

Symbol *symtab_lookup(const char *name) {
    unsigned int h = hash(name);
    Symbol *s = hash_table[h];
    while (s != NULL) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
        s = s->next;
    }
    return NULL;
}

void symtab_free(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        Symbol *s = hash_table[i];
        while (s != NULL) {
            Symbol *next = s->next;
            free(s);
            s = next;
        }
        hash_table[i] = NULL;
    }
}
