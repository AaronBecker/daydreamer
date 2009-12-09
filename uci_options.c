
#include "daydreamer.h"
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

typedef void(*option_handler)(void*, char*);

typedef struct {
    uci_option_type_t type;
    char name[128];
    char value[128];
    char vars[16][128];
    char default_value[128];
    int min;
    int max;
    option_handler handler;
} uci_option_t;

static int uci_option_count = 0;
static uci_option_t uci_options[128];

/*
 * Create an option and add it to the array. The set of things you have to
 * specify is a little weird to allow all option types to be handled with
 * the same function and data structure.
 */
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
    char option_command[256];
    sprintf(option_command, "%s value %s", option->name, option->default_value);
    set_uci_option(option_command);
}

/*
 * Convert an option into its uci description string and print it.
 */
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

/*
 * Dump all uci options to the console. Used during startup.
 */
void print_uci_options(void)
{
    for (int i=0; i<uci_option_count; ++i) {
        print_uci_option(&uci_options[i]);
    }
}

static uci_option_t* get_uci_option(const char* name)
{
    for (int i=0; i<uci_option_count; ++i) {
        int name_length = strlen(uci_options[i].name);
        if (!strncasecmp(name, uci_options[i].name, name_length)) {
            return &uci_options[i];
        }
    }
    return NULL;
}

/*
 * Handle a console command that sets a uci option. We receive the part of the
 * command that looks like:
 * <name> value <value>
 * Each option has its own handler callback which is invoked.
 */
void set_uci_option(char* command)
{
    while (isspace(*command)) ++command;
    uci_option_t* option = get_uci_option(command);
    if (!option) return;
    command = strcasestr(command, "value ");
    if (command) {
        command += 6;
        while(isspace(*command)) ++command;
    }
    option->handler(option, command);
}

int get_option_int(const char* name)
{
    uci_option_t* option = get_uci_option(name);
    assert(option);
    int value;
    sscanf(option->value, "%d", &value);
    return value;
}

char* get_option_string(const char* name)
{
    uci_option_t* option = get_uci_option(name);
    assert(option);
    return option->value;
}

bool get_option_bool(const char* name)
{
    uci_option_t* option = get_uci_option(name);
    assert(option);
    return strcasestr(option->value, "true") ? true : false;
}

static void default_handler(void* opt, char* value)
{
    uci_option_t* option = opt;
    if (value) strncpy(option->value, value, 128);
}

static void handle_hash(void* opt, char* value)
{
    uci_option_t* option = opt;
    int mbytes = 0;
    strncpy(option->value, value, 128);
    sscanf(value, "%d", &mbytes);
    if (mbytes < option->min || mbytes > option->max) {
        sscanf(option->default_value, "%d", &mbytes);
    }
    init_transposition_table(mbytes * (1<<20));
}

static void handle_clear_hash(void* opt, char* value)
{
    (void) opt; (void) value;
    clear_transposition_table();
}

static void handle_egbb_use(void* opt, char* value)
{
    uci_option_t* option = opt;
    strncpy(option->value, value, 128);
    if (!strcasecmp(value, "true")) {
        load_egbb(get_option_string("Endgame bitbase path"), 0);
    } else unload_egbb();
}

static void handle_egbb_path(void* opt, char* value)
{
    uci_option_t* option = opt;
    strncpy(option->value, value, 128);
    if (get_option_bool("Use endgame bitbases")) {
        load_egbb(value, 0);
    }
}

/*
 * Create all uci options and set them to their default values. Also set
 * default values for any options that aren't exposed to the uci interface.
 */
void init_uci_options()
{
    add_uci_option("Hash", OPTION_SPIN, "32", 1, 4096, NULL, &handle_hash);
    add_uci_option("Clear Hash", OPTION_BUTTON, "",
            0, 0, NULL, &handle_clear_hash);
    add_uci_option("Ponder", OPTION_CHECK, "false",
            0, 0, NULL, &default_handler);
    add_uci_option("MultiPV", OPTION_SPIN, "1",
            1, 256, NULL, &default_handler);
    add_uci_option("Use endgame bitbases", OPTION_CHECK, "false",
            0, 0, NULL, &handle_egbb_use);
    add_uci_option("Endgame bitbase path", OPTION_STRING, ".",
            0, 0, NULL, &handle_egbb_path);
    add_uci_option("Output Delay", OPTION_SPIN, "2000",
            0, 1000000, NULL, &default_handler);
    add_uci_option("Verbose output", OPTION_CHECK, "true",
            0, 0, NULL, &default_handler);
}

