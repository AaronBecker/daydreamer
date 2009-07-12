
#include <strings.h>
#include <stdio.h>
#include "daydreamer.h"

int main(void)
{
    // unbuffered i/o
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    init_daydreamer();
    char command[1024];
    position_t position;
    set_position(&position, FEN_STARTPOS);
    printf("daydreamer > ");
    while (fgets(command, 1024, stdin) != NULL) {
        char* strip = strrchr(command, '\n');
        if (strip) *strip = '\0';
        handle_console(&position, command);
        printf("daydreamer > ");
    }
}

