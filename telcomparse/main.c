#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include "executors.h"
#include "parsers.h"
#include "telcomparse.h"

#define COMMAND_MAXSIZE (128+1)

typedef const char* (*cmdparser_t)(const char*);
/* return 0 for success, nonzero for error */
typedef int (*cmdexecutor_t)(void);

cmdparser_t cmdparsers[256] = {};
cmdexecutor_t cmdexecutors[256] = {};
int purecommandlen;
char command[COMMAND_MAXSIZE];

cmdtype_e cmdtype;
uint8_t cmdnum;
telcomerr_e globerr;

static telcomerr_e parse(void)
{
    const char *c;

    int nibble;
    int checksum, calcchecksum;

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

    if(*c == ' ')
        c++;

    /* 
        this is a little dangerous if there is no
        cairrage return but the buffer has less than 8 bytes left,
        but since there can be a \0 in the crc
        i'm not sure we can do anything about it
    */
    if(*c != '\r')
    {
        calcchecksum = crc32(0, (Bytef*) command, c - command);
        checksum = *(long long int*) c;
        c += 8;

        if(calcchecksum != checksum)
            return ERR_CRC;
    }

    if(*c != '\r')
        return ERR_FORMAT;
    c++;

    return SUCCESS;
}

static telcomerr_e loadstrsafe(char* str)
{
    int len;
    int checksum;

    len = strlen(str);
    if(len >= COMMAND_MAXSIZE)
        return ERR_FORMAT;

    strcpy(command, str);

    purecommandlen = len;

    checksum = crc32(0, (Bytef*) command, len);
    *((int*)(command + len)) = checksum;
    len += 8;

    command[len++] = '\r';
    command[len] = 0;

    return SUCCESS;
}

void initparsers(void)
{
    #define X(enumid, name, id) cmdparsers[id] = cmdparser_##name;
    CMD_LIST
    #undef X
}

void initexecutors(void)
{
    #define X(enumid, name, id) cmdexecutors[id] = cmdexecutor_##name;
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
    initexecutors();

    for(i=1; i<argc; i++)
    {
        printf("---- command parse ----\n");
        if((code = loadstrsafe(argv[i])) != SUCCESS)
        {
            printf("error: %s\n\n", errstrs[code]);
            continue;
        }
        printf("command: %.*s\n", purecommandlen, command);
        if((code = parse()) != SUCCESS)
        {
            printf("error: %s\n\n", errstrs[code]);
            continue;
        }
        printf("type: %s\n", cmdtypestrs[cmdtype]);
        printf("command id: %"PRIX8"\n", cmdnum);
        if(cmdexecutors[cmdnum])
        {
            printf("command payload:\n");
            if((code = cmdexecutors[cmdnum]()))
            {
                printf("execution error: %s\n\n", errstrs[globerr]);
                continue;
            }
        }
        printf("\n");
    }
    
    return 0;
}