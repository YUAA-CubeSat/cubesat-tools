#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "parsers.h"
#include "telcomparse.h"

#define COMMAND_MAXSIZE 128

typedef const char* (*cmdparser_t)(const char*);

cmdparser_t cmdparsers[256] = {};
char command[COMMAND_MAXSIZE];

cmdtype_e cmdtype;
uint8_t cmdnum;
telcomerr_e globerr;

static telcomerr_e parse(void)
{
    const char *c;

    int nibble;

    c = command;
    
    if(strncmp(c, "YU+", 3))
        return ERR_FORMAT;
    c += 3;

    if(*c == 'R')
        cmdtype = CMDTYPE_READ;
    else if(*c == 'W')
        cmdtype = CMDTYPE_WRITE;
    else
        return ERR_FORMAT;
    c++;

    if((nibble = charhex(*c++)) == -1)
        return ERR_FORMAT;
    cmdnum = nibble << 4;
    if((nibble = charhex(*c++)) == -1)
        return ERR_FORMAT;
    cmdnum |= nibble;

    if(!cmdparsers[cmdnum])
        return ERR_UNSUPPORTED;

    globerr = SUCCESS;
    c = cmdparsers[cmdnum](c);
    if(!c)
        return globerr;

    return SUCCESS;
}

static telcomerr_e loadstrsafe(char* str)
{
    int len;

    len = strlen(str);
    if(len >= COMMAND_MAXSIZE)
        return ERR_FORMAT;

    strcpy(command, str);
    return SUCCESS;
}

void initparsers(void)
{
    #define X(enumid, name, id) cmdparsers[id] = cmdparser_##name;
    CMD_LIST
    #undef X
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

    initparsers();

    for(i=1; i<argc; i++)
    {
        printf("---- command parse ----\n");
        if((code = loadstrsafe(argv[i])) != SUCCESS)
        {
            printf("error: %s\n\n", errstrs[code]);
            continue;
        }
        printf("command: %s\n", command);
        if((code = parse()) != SUCCESS)
        {
            printf("error: %s\n\n", errstrs[code]);
            continue;
        }
        printf("type: %s\n", cmdtypestrs[cmdtype]);
        printf("command id: %"PRIX8"\n", cmdnum);
        printf("\n");
    }
    
    return 0;
}