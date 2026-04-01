#ifndef _TELCOMPARSE_H
#define _TELCOMPARSE_H

#include <stdint.h>

#define ERR_LIST \
    X(SUCCESS) \
    X(ERR_FORMAT) \
    X(ERR_INV_ARG) \
    X(ERR_CRC) \
    X(ERR_FAULT_X) \
    X(ERR_RES_LOCKED) \
    X(ERR_RUNTIME) \
    X(ERR_FORBIDDEN) \
    X(ERR_UNSUPPORTED)

#define CMDTYPE \
    X(CMDTYPE_READ) \
    X(CMDTYPE_WRITE) \

#define CMD_LIST \
    X(CMD_RECITEBEACON, recitebeacon, 0xF0) \
    X(CMD_SETPOWERBOUNDS, setpowerbounds, 0xF6) \
    X(CMD_SETPOWERTHRESH, setpowerthresh, 0xF7) \
    X(CMD_SETTEMPTHRESH, settempthresh, 0xF8) \
    \
    X(CMD_RADIOACK, radioack, 0xA0) \
    X(CMD_DOWNLINKREQ, downlinkreq, 0xA2) \
    X(CMD_LINKSPEED, linkspeed, 0xA8) \
    \
    X(CMD_SETCPUSPEED, setcpuspeed, 0x10) \
    X(CMD_OBCTIME, obctime, 0x11) \
    X(CMD_USERESETCOUNTS, useresetcounts, 0x12) \
    X(CMD_LOGSTATUS, logstatus, 0x15) \
    X(CMD_LOGSINRANGE, logsinrange, 0x16) \
    X(CMD_CLEARLOGRANGE, clearlogrange, 0x17) \
    X(CMD_SETPASSTIMES, setpasstimes, 0x18) \
    X(CMD_SETRADTIMEOUTS, setradtimeouts, 0x19) \
    X(CMD_SETGNDTIMEOUTS, setgndtimeouts, 0x1A) \
    \
    X(CMD_TOGGLEGGB, toggleggb, 0x30) \
    X(CMD_GGBEXTENSION, ggbextension, 0x31) \
    X(CMD_SETGGBTARGET, setggbtarget, 0x32) \
    X(CMD_SETGGBSPEED, setggbspeed, 0x33) \
    X(CMD_TOGGLESUNREQ, togglesunreq, 0x34) \
    \
    X(CMD_TOGGLEDETUMB, toggledetumb, 0x40) \
    X(CMD_SETMTQPOLAR, setmtqpolar, 0x41) \
    X(CMD_SETADCSTHRESH, setadcsthresh, 0x42) \
    X(CMD_SETSUNPOS, setsunpos, 0x43) \
    X(CMD_SETTLE, settle, 0x44) \
    X(CMD_GETATTMAT, getattmat, 0x45) \
    X(CMD_GETOMEGA, getomega, 0x46) \
    X(CMD_GETPOS, getpos, 0x47) \
    X(CMD_GETPHASE, getphase, 0x48) \
    X(CMD_GETBDOT, getbdot, 0x49) \
    X(CMD_GETBODYVECS, getbodyvecs, 0x4B) \
    X(CMD_SETMTQTIME, setmtqtime, 0x4E) \
    \
    X(CMD_SETPULSBINS, setpulsbins, 0x51) \

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
static const char* errstrs[NUMERR] = { ERR_LIST };
#undef X

#define X(type) #type,
static const char* cmdtypestrs[NUMCMDTYPE] = { CMDTYPE };
#undef X

extern cmdtype_e cmdtype;
extern uint8_t cmdnum;

extern telcomerr_e globerr;

#endif