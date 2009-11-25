
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
    printf("\n");
    init_daydreamer();

    // Read from uci script, if possible.
    FILE* script;
    if ((script = fopen("daydreamer.uci", "r"))) {
        uci_read_stream(script);
        fclose(script);
    }
    // Main loop input processing.
    uci_read_stream(stdin);
}

