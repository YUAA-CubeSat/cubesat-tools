#include <stdio.h>

#include <string.h>

#define COMMAND_MAXSIZE 128

char command[COMMAND_MAXSIZE];

/* return 1 if failed, 0 if success */
static int loadstrsafe(char* str)
{
    int len;

    len = strlen(str);
    if(len >= COMMAND_MAXSIZE)
        return 1;

    strcpy(command, str);
    return 0;
}

int main(int argc, char** argv)
{
    int i;

    printf("\n============ telcomparse ============\n\n");

    if(argc < 2)
    {
        printf("usage: telcomparse <commands>\n");
        return 1;
    }

    for(i=1; i<argc; i++)
    {
        printf("---- command parse ----\n");
        printf("command: %s\n", argv[i]);
        if(loadstrsafe(argv[i]))
        {
            printf("error: ERR_FORMAT\n\n");
            continue;
        }
        printf("\n");
    }
    
    return 0;
}