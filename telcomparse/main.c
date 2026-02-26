#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define COMMAND_MAXSIZE 128

#define ERR_LIST \
    X(SUCCESS) \
    X(ERR_FORMAT) \
    X(ERR_CRC) \
    X(ERR_FAULT_X) \
    X(ERR_RES_LOCKED) \
    X(ERR_RUNTIME) \
    X(ERR_FORBIDDEN) \
    X(ERR_UNSUPPORTED)

#define CMDTYPE \
    X(CMDTYPE_READ) \
    X(CMDTYPE_WRITE) \

typedef enum
{
#define X(code) code,
    ERR_LIST
#undef X
    NUMERR,
} telcomerr_e;

typedef enum
{
#define X(type) type,
    CMDTYPE
#undef X
    NUMCMDTYPE,
} cmdtype_e;

#define X(code) #code,
const char* errstrs[NUMERR] = { ERR_LIST };
#undef X

#define X(type) #type,
const char* cmdtypestrs[NUMCMDTYPE] = { CMDTYPE };
#undef X

char command[COMMAND_MAXSIZE];

cmdtype_e cmdtype;
uint8_t cmdnum;

telcomerr_e globerr;

typedef const char* (*cmdparser_t)(const char*);

cmdparser_t cmdparsers[256] = {};

const char* cmdparser_recitebeacon(const char* c)
{
    globerr = SUCCESS;

    if(*c++ != ' ')
    {
        globerr = ERR_FORMAT;
        return NULL;
    }

    return c;
}

/* -1 if invalid */
static int charhex(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

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
    cmdparsers[0xF0] = cmdparser_recitebeacon;
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