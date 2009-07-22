
#include "daydreamer.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    OPTION_CHECK,
    OPTION_SPIN,
    OPTION_COMBO,
    OPTION_BUTTON,
    OPTION_STRING
} uci_option_type_t;

typedef void(*option_handler)(void*, char*, search_options_t*);

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

static int uci_option_count = 0;
static uci_option_t uci_options[128];

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

void set_uci_option(char* command, search_options_t* options)
{
    // expect "<name> value <value>"
    while (isspace(*command)) ++command;
    for (int i=0; i<uci_option_count; ++i) {
        int name_length = strlen(uci_options[i].name);
        if (!strncasecmp(command, uci_options[i].name, name_length)) {
            command += name_length;
            while(isspace(*command)) ++command;
            uci_options[i].handler(&uci_options[i], command, options);
            return;
        }
    }
}

static void handle_hash(void* opt, char* value, search_options_t* options)
{
    (void)options;
    uci_option_t* option = opt;
    int mbytes = 0;
    sscanf(value, "value %d", &mbytes);
    if (mbytes < option->min || mbytes > option->max) {
        sscanf(option->default_value, "%d", &mbytes);
    }
    init_transposition_table(mbytes * (1<<20));
}

static void handle_output_delay(void* opt,
        char* value,
        search_options_t* options)
{
    uci_option_t* option = opt;
    int delay = 0;
    sscanf(value, "value %d", &delay);
    if (delay < option->min || delay > option->max) {
        sscanf(option->default_value, "%d", &delay);
    }
    options->output_delay = delay;
}

void init_uci_options(search_options_t* options)
{
    add_uci_option("Hash", OPTION_SPIN, "32", 1, 4096, NULL, &handle_hash);
    set_uci_option("Hash value 32", options);
    add_uci_option("Output Delay", OPTION_SPIN, "2000", 0, 1000000, NULL,
            &handle_output_delay);
    set_uci_option("Output Delay value 2000", options);
}
