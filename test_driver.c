
#include <strings.h>
#include <stdio.h>
#include "grasshopper.h"

int main(void)
{
    char command[1024];
    position_t position;
    set_position(&position,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    while (fgets(command, 1024, stdin) != NULL) {
        printf("grasshopper > ");
        handle_console(&position, command);
    }
}

