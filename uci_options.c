
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "daydreamer.h"

typedef enum {
    OPTION_CHECK,
    OPTION_SPIN,
    OPTION_COMBO,
    OPTION_BUTTON,
    OPTION_STRING
} uci_option_type_t;

typedef void(*option_handler)(void*, char*);

typedef struct {
    uci_option_type_t type;
    char name[128];
    char value[32];
    char vars[16][32];
    char default_value[32];
    int min;
    int max;
    option_handler handler;
} uci_option_t;

int uci_option_count = 0;
uci_option_t uci_options[128];

static void add_uci_option(char* name,
        uci_option_type_t type,
        char* default_value,
        int min,
        int max,
        char** vars,
        option_handler handler)
{
    assert(uci_option_count < 128);
    uci_option_t* option = &uci_options[uci_option_count++];
    strcpy(option->name, name);
    strcpy(option->default_value, default_value);
    strcpy(option->value, default_value);
    option->type = type;
    option->min = min;
    option->max = max;
    option->handler = handler;
    option->vars[0][0] = '\0';
    if (!vars) return;
    int var_index=0;
    while (*vars) {
        assert(var_index < 15);
        strcpy(option->vars[var_index++], *vars);
        ++*vars;
    }
    option->vars[var_index][0] = '\0';
}


static void print_uci_option(uci_option_t* option)
{
    char* option_types[] = { "check", "spin", "combo", "button", "string" };
    printf("option name %s type %s", option->name, option_types[option->type]);
    if (option->type == OPTION_BUTTON) {
        printf("\n");
        return;
    }
    printf(" default %s", option->default_value);
    if (option->type == OPTION_COMBO) {
        int var_index=0;
        while (option->vars[var_index]) {
            printf(" var %s", option->vars[var_index]);
        }
    } else if (option->type == OPTION_SPIN) {
        printf(" min %d max %d", option->min, option->max);
    }
    printf("\n");
}

void print_uci_options(void)
{
    for (int i=0; i<uci_option_count; ++i) {
        print_uci_option(&uci_options[i]);
    }
}

void set_uci_option(char* command)
{
    // expect "<name> value <value>
    while (isspace(*command)) ++command;
    for (int i=0; i<uci_option_count; ++i) {
        int name_length = strlen(uci_options[i].name);
        if (!strncasecmp(command, uci_options[i].name, name_length)) {
            uci_options[i].handler(&uci_options[i], command + name_length);
            return;
        }
    }
}

static void handle_hash(void* option, char* value)
{
    // do nothing, we don't even have a hash table.
    (void)option;
    (void)value;
}

void init_uci_options(void)
{
    add_uci_option("Hash", OPTION_SPIN, "1", 0, 4096, NULL, &handle_hash);
}
