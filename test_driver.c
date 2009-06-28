
#include <strings.h>
#include <stdio.h>
#include "grasshopper.h"

int main(void)
{
    grasshopper_init();
    char command[1024];
    position_t position;
    set_position(&position,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    printf("grasshopper > ");
    while (fgets(command, 1024, stdin) != NULL) {
        char* strip = strrchr(command, '\n');
        if (strip) *strip = '\0';
        handle_console(&position, command);
        printf("grasshopper > ");
    }
}

