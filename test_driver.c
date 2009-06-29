
#include <strings.h>
#include <stdio.h>
#include "grasshopper.h"

int main(void)
{
    // unbuffered i/o
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    grasshopper_init();
    char command[1024];
    position_t position;
    set_position(&position, FEN_STARTPOS);
    printf("grasshopper > ");
    while (fgets(command, 1024, stdin) != NULL) {
        char* strip = strrchr(command, '\n');
        if (strip) *strip = '\0';
        handle_console(&position, command);
        printf("grasshopper > ");
    }
}

