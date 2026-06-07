#ifndef SYMTAB_H
#define SYMTAB_H

#define MAX_DOMAIN_LEN 64
#define MAX_NAME_LEN 256

typedef struct Symbol {
    char name[MAX_NAME_LEN];
    char domain[MAX_DOMAIN_LEN];
    struct Symbol *next;
} Symbol;

void symtab_init(void);
void symtab_insert(const char *name, const char *domain);
Symbol *symtab_lookup(const char *name);
void symtab_free(void);

#endif
