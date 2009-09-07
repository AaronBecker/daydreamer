
#include "daydreamer.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

int main(void)
{
    // Set unbuffered i/o.
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    printf("%s %s, by %s\n", ENGINE_NAME, ENGINE_VERSION, ENGINE_AUTHOR);
    printf("Compiled %s %s", __DATE__, __TIME__);
#ifdef COMPILE_COMMAND
    printf(", using:\n%s", COMPILE_COMMAND);
#endif
    printf("\ninitializing engine...\n");
    init_daydreamer();
    char command[4096];
    position_t position;
    set_position(&position, FEN_STARTPOS);
    printf("\ndaydreamer > ");
    while (fgets(command, 4096, stdin) != NULL) {
        char* strip = strrchr(command, '\n');
        if (strip) *strip = '\0';
        handle_console(&position, command);
        printf("daydreamer > ");
    }
}

