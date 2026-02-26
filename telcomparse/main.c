#include <stdio.h>

#include <string.h>

#define COMMAND_MAXSIZE 128

char command[COMMAND_MAXSIZE];

#define ERR_LIST \
    X(SUCCESS) \
    X(ERR_FORMAT) \
    X(ERR_CRC) \
    X(ERR_FAULT_X) \
    X(ERR_RES_LOCKED) \
    X(ERR_RUNTIME) \
    X(ERR_FORBIDDEN) \
    X(ERR_UNSUPPORTED)

typedef enum
{
#define X(code) code,
    ERR_LIST
#undef X
    NUMERR,
} telcomerr_e;

#define X(code) #code,
const char* errstrs[NUMERR] = { ERR_LIST };
#undef X

static telcomerr_e loadstrsafe(char* str)
{
    int len;

    len = strlen(str);
    if(len >= COMMAND_MAXSIZE)
        return ERR_FORMAT;

    strcpy(command, str);
    return SUCCESS;
}

int main(int argc, char** argv)
{
    int i;

    telcomerr_e code;

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
        if((code = loadstrsafe(argv[i])) != SUCCESS)
        {
            printf("code: %s\n\n", errstrs[code]);
            continue;
        }
        printf("\n");
    }
    
    return 0;
}