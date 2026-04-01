#include "parsers.h"

#include <stdlib.h>
#include <string.h>

#define PARSERR(err) { globerr = err; return NULL; }

parsed_t parsed;

/* -1 if invalid */
int charhex(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static float parserawfloat(const char* c)
{
    float f;
    memcpy(&f, c, sizeof(float));
    return f;
}

static uint64_t parserawu64(const char* c)
{
    uint64_t v;
    memcpy(&v, c, sizeof(uint64_t));
    return v;
}

const char* cmdparser_recitebeacon(const char* c)
{
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setpowerbounds(const char* c)
{
    int i;

    setpowerbounds_t *info;
    int digit;

    info = (void*) &parsed;

    if(*c != '0' && *c != '1')
        PARSERR(ERR_FORMAT)
    info->which = *c++ - '0';

    if(*c++ != 'L')
        PARSERR(ERR_FORMAT)

    if(*c < '0' || *c > '9')
        PARSERR(ERR_FORMAT)
    info->level = *c++ - '0';

    if(*c++ != '+')
        PARSERR(ERR_FORMAT)

    info->duration = 0;
    for(i=0; i<4; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->duration |= digit << (12-i*4);
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setpowerthresh(const char* c)
{
    /* spec says TODO */
    return c;
}

const char* cmdparser_settempthresh(const char* c)
{
    /* W only: YU+WF8 [C..C] - 128 bytes of temp data follow in a second packet */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_radioack(const char* c)
{
    /* arbitrary-format packet, not ESTTC; not parsed here */
    return c;
}

const char* cmdparser_downlinkreq(const char* c)
{
    /* AX.25 UI format, not ESTTC; not parsed here */
    return c;
}

const char* cmdparser_linkspeed(const char* c)
{
    /* YU+WA8[SS] [C..C] */
    int i;

    linkspeed_t *info;
    int digit;

    info = (void*) &parsed;

    info->rfmode = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->rfmode |= digit << (4-i*4);
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setcpuspeed(const char* c)
{
    /* YU+W10[FF] [C..C] */
    int i;

    setcpuspeed_t *info;
    int digit;

    info = (void*) &parsed;

    info->freqmhz = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->freqmhz |= digit << (4-i*4);
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_obctime(const char* c)
{
    /* R: YU+R11 [C..C]
     * W: YU+W11[TTTTTTTT] [C..C]  - [TTTTTTTT] is 8 raw bytes */
    obctime_t *info;

    info = (void*) &parsed;

    if(cmdtype == CMDTYPE_READ)
    {
        if(*c++ != ' ')
            PARSERR(ERR_FORMAT)
    }
    else
    {
        info->timestamp = parserawu64(c);
        c += 8;
        if(*c++ != ' ')
            PARSERR(ERR_FORMAT)
    }

    return c;
}

const char* cmdparser_useresetcounts(const char* c)
{
    /* R: YU+R12 [C..C]
     * W: YU+W12 [C..C]  - no inline args, clears all counters */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_logstatus(const char* c)
{
    /* YU+R15 [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_logsinrange(const char* c)
{
    /* YU+R16[LL]+[SSSSSSSS]+[EEEEEEEE] [C..C]
     * [LL] = 2 hex digits, [SSSSSSSS]/[EEEEEEEE] = 8 raw bytes each */
    int i;

    logsinrange_t *info;
    int digit;

    info = (void*) &parsed;

    info->logtype = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->logtype |= digit << (4-i*4);
    }

    if(*c++ != '+')
        PARSERR(ERR_FORMAT)
    info->starttime = parserawu64(c);
    c += 8;

    if(*c++ != '+')
        PARSERR(ERR_FORMAT)
    info->endtime = parserawu64(c);
    c += 8;

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_clearlogrange(const char* c)
{
    /* YU+W17[LL]+[SSSSSSSS]+[EEEEEEEE] [C..C] */
    int i;

    clearlogrange_t *info;
    int digit;

    info = (void*) &parsed;

    info->logtype = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->logtype |= digit << (4-i*4);
    }

    if(*c++ != '+')
        PARSERR(ERR_FORMAT)
    info->starttime = parserawu64(c);
    c += 8;

    if(*c++ != '+')
        PARSERR(ERR_FORMAT)
    info->endtime = parserawu64(c);
    c += 8;

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setpasstimes(const char* c)
{
    /* YU+W18[NN] [C..C] */
    int i;

    setpasstimes_t *info;
    int digit;

    info = (void*) &parsed;

    info->numpasses = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->numpasses |= digit << (4-i*4);
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setradtimeouts(const char* c)
{
    /* YU+W19[AA]+[BB]+[FF]+[DD] [C..C] */
    int i;
    int j;

    setradtimeouts_t *info;
    uint8_t *fields[4];
    int digit;

    info = (void*) &parsed;
    fields[0] = &info->cmdtimeoutmin;
    fields[1] = &info->activewindowmin;
    fields[2] = &info->passivewindowmin;
    fields[3] = &info->recoverytimeoutdays;

    for(j=0; j<4; j++)
    {
        *fields[j] = 0;
        for(i=0; i<2; i++, c++)
        {
            digit = charhex(*c);
            if(digit == -1)
                PARSERR(ERR_FORMAT)
            *fields[j] |= digit << (4-i*4);
        }
        if(j < 3 && *c++ != '+')
            PARSERR(ERR_FORMAT)
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setgndtimeouts(const char* c)
{
    /* YU+W1A[NN] [C..C] */
    int i;

    setgndtimeouts_t *info;
    int digit;

    info = (void*) &parsed;

    info->numcoords = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->numcoords |= digit << (4-i*4);
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_toggleggb(const char* c)
{
    /* YU+W30[E] [C..C] */
    toggleggb_t *info;

    info = (void*) &parsed;

    if(*c != '0' && *c != '1')
        PARSERR(ERR_FORMAT)
    info->enable = *c++ - '0';

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_ggbextension(const char* c)
{
    /* R: YU+R31 [C..C]
     * W: YU+W31+[EEEEE] [C..C]  - unsigned decimal float */
    ggbextension_t *info;
    char *end;

    info = (void*) &parsed;

    if(cmdtype == CMDTYPE_READ)
    {
        if(*c++ != ' ')
            PARSERR(ERR_FORMAT)
    }
    else
    {
        if(*c++ != '+')
            PARSERR(ERR_FORMAT)
        info->extension = (float)strtod(c, &end);
        if(end == c || info->extension < 0.0f)
            PARSERR(ERR_INV_ARG)
        c = end;
        if(*c++ != ' ')
            PARSERR(ERR_FORMAT)
    }

    return c;
}

const char* cmdparser_setggbtarget(const char* c)
{
    /* YU+W32[EEEEE] [C..C]  - unsigned decimal float */
    setggbtarget_t *info;
    char *end;

    info = (void*) &parsed;

    info->target = (float)strtod(c, &end);
    if(end == c || info->target < 0.0f)
        PARSERR(ERR_INV_ARG)
    c = end;

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setggbspeed(const char* c)
{
    /* YU+W33[SS] [C..C] */
    int i;

    setggbspeed_t *info;
    int digit;

    info = (void*) &parsed;

    info->speed = 0;
    for(i=0; i<2; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->speed |= digit << (4-i*4);
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_togglesunreq(const char* c)
{
    /* YU+W34[S] [C..C] */
    togglesunreq_t *info;

    info = (void*) &parsed;

    if(*c != '0' && *c != '1')
        PARSERR(ERR_FORMAT)
    info->requiresun = *c++ - '0';

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setsunpos(const char* c)
{
    /* YU+W43[TTTTTTTTTTTTTTTT] [C..C]  - 8-byte timestamp as 16 hex digits */
    int i;

    setsunpos_t *info;
    int digit;

    info = (void*) &parsed;

    info->firstvectime = 0;
    for(i=0; i<16; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->firstvectime = (info->firstvectime << 4) | (uint64_t)digit;
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_toggledetumb(const char* c)
{
    /* YU+W40[E] [C..C] */
    toggledetumb_t *info;

    info = (void*) &parsed;

    if(*c != '0' && *c != '1')
        PARSERR(ERR_FORMAT)
    info->enable = *c++ - '0';

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setmtqpolar(const char* c)
{
    /* YU+W41[P] [C..C] */
    setmtqpolar_t *info;

    info = (void*) &parsed;

    if(*c != '0' && *c != '1')
        PARSERR(ERR_FORMAT)
    info->invert = *c++ - '0';

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setadcsthresh(const char* c)
{
    /* YU+W42+[DDDD]+[EEEE]+[FFFF]+[GGGG]+[MMMM]+[NNNN] [C..C]
     * each [XXXX] is 4 raw bytes (float, not printable) */
    int i;

    setadcsthresh_t *info;
    float *fields[6];

    info = (void*) &parsed;
    fields[0] = &info->detstopb;
    fields[1] = &info->detstopomega;
    fields[2] = &info->detresumeb;
    fields[3] = &info->detresumeomega;
    fields[4] = &info->critb;
    fields[5] = &info->critomega;

    for(i=0; i<6; i++)
    {
        if(*c++ != '+')
            PARSERR(ERR_FORMAT)
        *fields[i] = parserawfloat(c);
        c += 4;
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_settle(const char* c)
{
    /* YU+W44 [C..C]  - TLE lines come in follow-up packets */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_getattmat(const char* c)
{
    /* YU+R45 [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_getomega(const char* c)
{
    /* YU+R46 [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_getpos(const char* c)
{
    /* YU+R47 [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_getphase(const char* c)
{
    /* YU+R48 [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_getbdot(const char* c)
{
    /* YU+R49 [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_getbodyvecs(const char* c)
{
    /* YU+R4B [C..C] */
    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setmtqtime(const char* c)
{
    /* YU+W4E+[TTTT];[+XX];[+YY];[+ZZ] [C..C]
     * [TTTT] = 4 hex digits (seconds, must be > 0)
     * [+XX/YY/ZZ] = sign + 2 hex digits, value in [-100, +100] */
    int i;
    int j;

    setmtqtime_t *info;
    int8_t *components[3];
    int digit;
    int sign;
    int val;

    info = (void*) &parsed;
    components[0] = &info->xpct;
    components[1] = &info->ypct;
    components[2] = &info->zpct;

    if(*c++ != '+')
        PARSERR(ERR_FORMAT)

    info->durationsec = 0;
    for(i=0; i<4; i++, c++)
    {
        digit = charhex(*c);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        info->durationsec |= digit << (12-i*4);
    }
    if(info->durationsec == 0)
        PARSERR(ERR_INV_ARG)

    for(j=0; j<3; j++)
    {
        if(*c++ != ';')
            PARSERR(ERR_FORMAT)
        if(*c == '+')       sign =  1;
        else if(*c == '-')  sign = -1;
        else                PARSERR(ERR_FORMAT)
        c++;
        digit = charhex(*c++);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        val = digit << 4;
        digit = charhex(*c++);
        if(digit == -1)
            PARSERR(ERR_FORMAT)
        val = (val | digit) * sign;
        if(val < -100 || val > 100)
            PARSERR(ERR_INV_ARG)
        *components[j] = (int8_t)val;
    }

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}

const char* cmdparser_setpulsbins(const char* c)
{
    /* YU+W51[DAC output data] [C..C]
     * 8 contiguous raw bytes: DAC_1 out0..out3, DAC_2 out0..out3 */
    int i;

    setpulsbins_t *info;

    info = (void*) &parsed;

    for(i=0; i<8; i++)
        info->dacoutputs[i] = (uint8_t)*c++;

    if(*c++ != ' ')
        PARSERR(ERR_FORMAT)

    return c;
}
