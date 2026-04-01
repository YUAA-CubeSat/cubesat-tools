#ifndef _PARSERS_H
#define _PARSERS_H

#include "telcomparse.h"

#define X(enumid, name, id) typedef struct name##_s name##_t;
CMD_LIST
#undef X

struct recitebeacon_s
{
    /* no inline args */
};

struct setpowerbounds_s
{
    int which;    /* 0 = floor, 1 = ceiling */
    int level;    /* system power level digit [0,9] */
    int duration; /* duration in minutes; 0 = indefinite */
};

struct setpowerthresh_s
{
    /* spec says TODO */
};

struct settempthresh_s
{
    /* no inline args; 128 bytes of temp data come in a follow-up packet */
};

struct radioack_s
{
    /* arbitrary-format packet, not ESTTC */
};

struct downlinkreq_s
{
    /* AX.25 UI format, not ESTTC */
};

struct linkspeed_s
{
    uint8_t rfmode; /* RF mode per TCV manual Table 11 */
};

struct setcpuspeed_s
{
    uint8_t freqmhz; /* target CPU clock frequency in MHz */
};

struct obctime_s
{
    uint64_t timestamp; /* Unix timestamp as 8 raw bytes; only populated for W */
};

struct useresetcounts_s
{
    /* no inline args */
};

struct logstatus_s
{
    /* no args */
};

struct logsinrange_s
{
    uint8_t  logtype;    /* log type per Memory_Logs.h::LOG_TYPE */
    uint64_t starttime;  /* start of reporting period as 8 raw bytes */
    uint64_t endtime;    /* end of reporting period as 8 raw bytes */
};

struct clearlogrange_s
{
    uint8_t  logtype;
    uint64_t starttime;
    uint64_t endtime;
};

struct setpasstimes_s
{
    uint8_t numpasses; /* number of pass times to be uplinked */
};

struct setradtimeouts_s
{
    uint8_t cmdtimeoutmin;       /* [AA] command timeout in minutes */
    uint8_t activewindowmin;     /* [BB] active pass window in minutes */
    uint8_t passivewindowmin;    /* [FF] passive pass window in minutes */
    uint8_t recoverytimeoutdays; /* [DD] recovery mode timeout in days */
};

struct setgndtimeouts_s
{
    uint8_t numcoords; /* number of lat-long coordinate pairs to be uplinked */
};

struct toggleggb_s
{
    int enable; /* 1 = enable GGB movement, 0 = disable */
};

struct ggbextension_s
{
    float extension; /* override extension in meters; only populated for W */
};

struct setggbtarget_s
{
    float target; /* extension target in meters */
};

struct setggbspeed_s
{
    uint8_t speed; /* GGB motor speed byte */
};

struct togglesunreq_s
{
    int requiresun; /* 1 = require sun detection to move GGB, 0 = do not */
};

struct setsunpos_s
{
    uint64_t firstvectime; /* Unix timestamp of first sun vector, as 16 hex digits */
};

struct toggledetumb_s
{
    int enable; /* 1 = enable active detumbling, 0 = disable */
};

struct setmtqpolar_s
{
    int invert; /* 0 = normal polarity, 1 = inverted */
};

struct setadcsthresh_s
{
    float detstopb;       /* [DDDD] Detumbling Stop B rotation threshold (rad/s) */
    float detstopomega;   /* [EEEE] Detumbling Stop omega projection threshold (rad/s) */
    float detresumeb;     /* [FFFF] Detumbling Resume B rotation threshold (rad/s) */
    float detresumeomega; /* [GGGG] Detumbling Resume omega projection threshold (rad/s) */
    float critb;          /* [MMMM] Critical B rotation threshold (rad/s) */
    float critomega;      /* [NNNN] Critical omega projection threshold (rad/s) */
};

struct settle_s
{
    /* no inline args; TLE lines come in follow-up packets */
};

struct getattmat_s
{
    /* no args */
};

struct getomega_s
{
    /* no args */
};

struct getpos_s
{
    /* no args */
};

struct getphase_s
{
    /* no args */
};

struct getbdot_s
{
    /* no args */
};

struct getbodyvecs_s
{
    /* no args */
};

struct setmtqtime_s
{
    uint16_t durationsec; /* [TTTT] output duration in seconds (must be > 0) */
    int8_t   xpct;        /* [+XX] X component in percent, -100 to +100 */
    int8_t   ypct;        /* [+YY] Y component in percent, -100 to +100 */
    int8_t   zpct;        /* [+ZZ] Z component in percent, -100 to +100 */
};

struct setpulsbins_s
{
    uint8_t dacoutputs[8]; /* DAC_1 out0..out3, DAC_2 out0..out3 (raw bytes) */
};

typedef union
{
    #define X(enumid, name, id) name##_t name;
    CMD_LIST
    #undef X
} parsed_t;

extern parsed_t parsed;

/* -1 if invalid */
int charhex(char c);

#define X(enumid, name, id) const char* cmdparser_##name(const char* c);
CMD_LIST
#undef X

#endif
