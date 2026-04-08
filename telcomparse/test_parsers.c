/*
 * test_parsers.c  --  unit tests for every telcomparse command parser
 *
 * Build via:  make test_telcomparse
 * Run  via:  ./bin/test_telcomparse
 *
 * Each test_XXX() function covers one command parser.  For each parser the
 * suite checks:
 *   - every valid input class (boundary values, typical values)
 *   - every distinct error path (ERR_FORMAT, ERR_INV_ARG, …)
 *
 * Parsers are called directly so the test does not depend on the CRC logic
 * or the full parse() dispatcher that lives in main.c.
 */

#include <stdio.h>
#include <string.h>

#include "parsers.h"
#include "telcomparse.h"

/* globals required by parsers.c (normally defined in main.c) */
cmdtype_e   cmdtype;
uint8_t     cmdnum;
telcomerr_e globerr;

/* ------------------------------------------------------------------ */
/* minimal test framework                                               */
/* ------------------------------------------------------------------ */

static int pass_count = 0;
static int fail_count = 0;

#define CHECK(cond, label) \
    do { \
        if(cond) { \
            pass_count++; \
        } else { \
            fprintf(stderr, "FAIL [%s] line %d: %s\n", (label), __LINE__, #cond); \
            fail_count++; \
        } \
    } while(0)

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

static const char* parse_w(const char* (*parser)(const char*), const char* input)
{
    cmdtype = CMDTYPE_WRITE;
    globerr  = SUCCESS;
    return parser(input);
}

static const char* parse_r(const char* (*parser)(const char*), const char* input)
{
    cmdtype = CMDTYPE_READ;
    globerr  = SUCCESS;
    return parser(input);
}

static void put_float(char* buf, float f)  { memcpy(buf, &f, 4); }
static void put_u64(char* buf, uint64_t v) { memcpy(buf, &v, 8); }

/* ================================================================== */
/* recitebeacon  0xF0                                                   */
/* ================================================================== */

static void test_recitebeacon(void)
{
    const char *end;

    /* optional leading space consumed */
    end = parse_r(cmdparser_recitebeacon, " \r");
    CHECK(end != NULL,   "rb/space");
    CHECK(*end == '\r',  "rb/space_pos");

    /* no space */
    end = parse_r(cmdparser_recitebeacon, "\r");
    CHECK(end != NULL,   "rb/nospace");
    CHECK(*end == '\r',  "rb/nospace_pos");
}

/* ================================================================== */
/* setpowerbounds  0xF6   YU+W/RF6[S]L[P]+[MMMM]                      */
/* ================================================================== */

static void test_setpowerbounds(void)
{
    const char *end;

    /* floor, level 0, duration 0 */
    end = parse_w(cmdparser_setpowerbounds, "0L0+0000 \r");
    CHECK(end != NULL,                          "spb/floor0");
    CHECK(globerr == SUCCESS,                   "spb/floor0_err");
    CHECK(parsed.setpowerbounds.which    == 0,  "spb/floor0_which");
    CHECK(parsed.setpowerbounds.level    == 0,  "spb/floor0_level");
    CHECK(parsed.setpowerbounds.duration == 0,  "spb/floor0_dur");

    /* ceiling, level 4, duration 0xFFFF = 65535 min */
    end = parse_w(cmdparser_setpowerbounds, "1L4+FFFF \r");
    CHECK(end != NULL,                              "spb/ceil4");
    CHECK(parsed.setpowerbounds.which    == 1,      "spb/ceil4_which");
    CHECK(parsed.setpowerbounds.level    == 4,      "spb/ceil4_level");
    CHECK(parsed.setpowerbounds.duration == 0xFFFF, "spb/ceil4_dur");

    /* floor, level 9, duration 0x1234 */
    end = parse_w(cmdparser_setpowerbounds, "0L9+1234\r");
    CHECK(end != NULL,                               "spb/l9");
    CHECK(parsed.setpowerbounds.level    == 9,       "spb/l9_level");
    CHECK(parsed.setpowerbounds.duration == 0x1234,  "spb/l9_dur");

    /* ERR_FORMAT: which not 0 or 1 */
    end = parse_w(cmdparser_setpowerbounds, "2L0+0000\r");
    CHECK(end == NULL,           "spb/err_which");
    CHECK(globerr == ERR_FORMAT, "spb/err_which_code");

    /* ERR_FORMAT: 'L' missing */
    end = parse_w(cmdparser_setpowerbounds, "0X0+0000\r");
    CHECK(end == NULL,           "spb/err_noL");
    CHECK(globerr == ERR_FORMAT, "spb/err_noL_code");

    /* ERR_FORMAT: level not a decimal digit */
    end = parse_w(cmdparser_setpowerbounds, "0LX+0000\r");
    CHECK(end == NULL,           "spb/err_level");
    CHECK(globerr == ERR_FORMAT, "spb/err_level_code");

    /* ERR_FORMAT: '+' missing before duration */
    end = parse_w(cmdparser_setpowerbounds, "0L0X0000\r");
    CHECK(end == NULL,           "spb/err_noplus");
    CHECK(globerr == ERR_FORMAT, "spb/err_noplus_code");

    /* ERR_FORMAT: non-hex duration digits */
    end = parse_w(cmdparser_setpowerbounds, "0L0+GGGG\r");
    CHECK(end == NULL,           "spb/err_dur_hex");
    CHECK(globerr == ERR_FORMAT, "spb/err_dur_hex_code");
}

/* ================================================================== */
/* linkspeed  0xA8   YU+WA8[SS]                                        */
/* ================================================================== */

static void test_linkspeed(void)
{
    const char *end;

    end = parse_w(cmdparser_linkspeed, "00 \r");
    CHECK(end != NULL,                     "ls/00");
    CHECK(parsed.linkspeed.rfmode == 0x00, "ls/00_val");

    end = parse_w(cmdparser_linkspeed, "FF \r");
    CHECK(end != NULL,                     "ls/FF");
    CHECK(parsed.linkspeed.rfmode == 0xFF, "ls/FF_val");

    end = parse_w(cmdparser_linkspeed, "A5\r");
    CHECK(end != NULL,                     "ls/A5");
    CHECK(parsed.linkspeed.rfmode == 0xA5, "ls/A5_val");

    /* ERR_FORMAT: invalid first nibble */
    end = parse_w(cmdparser_linkspeed, "GF\r");
    CHECK(end == NULL,           "ls/err_first");
    CHECK(globerr == ERR_FORMAT, "ls/err_first_code");

    /* ERR_FORMAT: invalid second nibble */
    end = parse_w(cmdparser_linkspeed, "0G\r");
    CHECK(end == NULL,           "ls/err_second");
    CHECK(globerr == ERR_FORMAT, "ls/err_second_code");
}

/* ================================================================== */
/* setcpuspeed  0x10   YU+W10[FF]                                      */
/* ================================================================== */

static void test_setcpuspeed(void)
{
    const char *end;

    end = parse_w(cmdparser_setcpuspeed, "00\r");
    CHECK(end != NULL,                      "cpu/00");
    CHECK(parsed.setcpuspeed.freqmhz == 0,  "cpu/00_val");

    end = parse_w(cmdparser_setcpuspeed, "FF\r");
    CHECK(end != NULL,                        "cpu/FF");
    CHECK(parsed.setcpuspeed.freqmhz == 0xFF, "cpu/FF_val");

    end = parse_w(cmdparser_setcpuspeed, "40\r");
    CHECK(end != NULL,                        "cpu/40");
    CHECK(parsed.setcpuspeed.freqmhz == 0x40, "cpu/40_val");

    end = parse_w(cmdparser_setcpuspeed, "GG\r");
    CHECK(end == NULL,           "cpu/err");
    CHECK(globerr == ERR_FORMAT, "cpu/err_code");
}

/* ================================================================== */
/* obctime  0x11                                                        */
/*   R: YU+R11                                                          */
/*   W: YU+W11[TTTTTTTT]  (8 raw bytes)                               */
/* ================================================================== */

static void test_obctime(void)
{
    const char *end;
    char buf[32];
    uint64_t ts;

    /* read: skip optional space */
    end = parse_r(cmdparser_obctime, " \r");
    CHECK(end != NULL,  "obc/read");
    CHECK(*end == '\r', "obc/read_pos");

    /* write: typical Unix timestamp */
    ts = (uint64_t)0x0000000060000000ULL;
    put_u64(buf, ts);
    buf[8] = ' '; buf[9] = '\r'; buf[10] = '\0';
    end = parse_w(cmdparser_obctime, buf);
    CHECK(end != NULL,                    "obc/write");
    CHECK(parsed.obctime.timestamp == ts, "obc/write_val");

    /* write: zero */
    put_u64(buf, (uint64_t)0ULL);
    buf[8] = '\r'; buf[9] = '\0';
    end = parse_w(cmdparser_obctime, buf);
    CHECK(end != NULL,                          "obc/write_zero");
    CHECK(parsed.obctime.timestamp == 0ULL,     "obc/write_zero_val");

    /* write: max */
    put_u64(buf, (uint64_t)0xFFFFFFFFFFFFFFFFULL);
    buf[8] = '\r'; buf[9] = '\0';
    end = parse_w(cmdparser_obctime, buf);
    CHECK(end != NULL,                                         "obc/write_max");
    CHECK(parsed.obctime.timestamp == 0xFFFFFFFFFFFFFFFFULL,   "obc/write_max_val");
}

/* ================================================================== */
/* useresetcounts  0x12  (no inline args for both R and W)             */
/* ================================================================== */

static void test_useresetcounts(void)
{
    const char *end;

    end = parse_w(cmdparser_useresetcounts, " \r");
    CHECK(end != NULL, "urc/write");

    end = parse_r(cmdparser_useresetcounts, "\r");
    CHECK(end != NULL, "urc/read");
}

/* ================================================================== */
/* logstatus  0x15   YU+R15  (no args)                                 */
/* ================================================================== */

static void test_logstatus(void)
{
    const char *end;

    end = parse_r(cmdparser_logstatus, " \r");
    CHECK(end != NULL, "ls15/space");

    end = parse_r(cmdparser_logstatus, "\r");
    CHECK(end != NULL, "ls15/nospace");
}

/* ================================================================== */
/* logsinrange  0x16   YU+R16[LL]+[SSSSSSSS]+[EEEEEEEE]               */
/* ================================================================== */

static void test_logsinrange(void)
{
    const char *end;
    char buf[32];
    uint64_t t0, t1;

    /* valid: logtype 0x01, typical window */
    t0 = (uint64_t)0x0000000000000100ULL;
    t1 = (uint64_t)0x0000000000000200ULL;
    buf[0] = '0'; buf[1] = '1'; buf[2] = '+';
    put_u64(buf+3,  t0);
    buf[11] = '+';
    put_u64(buf+12, t1);
    buf[20] = ' '; buf[21] = '\r'; buf[22] = '\0';
    end = parse_r(cmdparser_logsinrange, buf);
    CHECK(end != NULL,                         "lir/valid");
    CHECK(parsed.logsinrange.logtype    == 0x01, "lir/logtype");
    CHECK(parsed.logsinrange.starttime  == t0,   "lir/start");
    CHECK(parsed.logsinrange.endtime    == t1,   "lir/end");

    /* valid: logtype 0xFF, UINT64_MAX sentinel end */
    buf[0] = 'F'; buf[1] = 'F'; buf[2] = '+';
    put_u64(buf+3,  (uint64_t)0ULL);
    buf[11] = '+';
    put_u64(buf+12, (uint64_t)0xFFFFFFFFFFFFFFFFULL);
    buf[20] = '\r'; buf[21] = '\0';
    end = parse_r(cmdparser_logsinrange, buf);
    CHECK(end != NULL,                           "lir/FF");
    CHECK(parsed.logsinrange.logtype    == 0xFF, "lir/FF_type");

    /* ERR_FORMAT: invalid hex in logtype */
    buf[0] = 'G'; buf[1] = '1'; buf[2] = '+';
    end = parse_r(cmdparser_logsinrange, buf);
    CHECK(end == NULL,           "lir/err_lt");
    CHECK(globerr == ERR_FORMAT, "lir/err_lt_code");

    /* ERR_FORMAT: missing first '+' */
    buf[0] = '0'; buf[1] = '1'; buf[2] = 'X';
    end = parse_r(cmdparser_logsinrange, buf);
    CHECK(end == NULL,           "lir/err_plus1");
    CHECK(globerr == ERR_FORMAT, "lir/err_plus1_code");

    /* ERR_FORMAT: missing second '+' */
    buf[0] = '0'; buf[1] = '1'; buf[2] = '+';
    put_u64(buf+3, t0);
    buf[11] = 'X';
    end = parse_r(cmdparser_logsinrange, buf);
    CHECK(end == NULL,           "lir/err_plus2");
    CHECK(globerr == ERR_FORMAT, "lir/err_plus2_code");
}

/* ================================================================== */
/* clearlogrange  0x17   YU+W17[LL]+[SSSSSSSS]+[EEEEEEEE]             */
/* ================================================================== */

static void test_clearlogrange(void)
{
    const char *end;
    char buf[32];
    uint64_t t0, t1;

    t0 = (uint64_t)0x00000000ABCDEF00ULL;
    t1 = (uint64_t)0xFFFFFFFFFFFFFFFFULL;
    buf[0] = '0'; buf[1] = '2'; buf[2] = '+';
    put_u64(buf+3,  t0);
    buf[11] = '+';
    put_u64(buf+12, t1);
    buf[20] = '\r'; buf[21] = '\0';
    end = parse_w(cmdparser_clearlogrange, buf);
    CHECK(end != NULL,                            "clr/valid");
    CHECK(parsed.clearlogrange.logtype    == 0x02, "clr/logtype");
    CHECK(parsed.clearlogrange.starttime  == t0,   "clr/start");
    CHECK(parsed.clearlogrange.endtime    == t1,   "clr/end");

    /* ERR_FORMAT: invalid hex */
    buf[0] = 'Z';
    end = parse_w(cmdparser_clearlogrange, buf);
    CHECK(end == NULL,           "clr/err");
    CHECK(globerr == ERR_FORMAT, "clr/err_code");
}

/* ================================================================== */
/* setpasstimes  0x18   YU+W18[NN]                                     */
/* ================================================================== */

static void test_setpasstimes(void)
{
    const char *end;

    end = parse_w(cmdparser_setpasstimes, "05\r");
    CHECK(end != NULL,                        "spt/5");
    CHECK(parsed.setpasstimes.numpasses == 5, "spt/5_val");

    end = parse_w(cmdparser_setpasstimes, "FF\r");
    CHECK(end != NULL,                          "spt/FF");
    CHECK(parsed.setpasstimes.numpasses == 255, "spt/FF_val");

    end = parse_w(cmdparser_setpasstimes, "00\r");
    CHECK(end != NULL,                         "spt/00");
    CHECK(parsed.setpasstimes.numpasses == 0,  "spt/00_val");

    end = parse_w(cmdparser_setpasstimes, "GG\r");
    CHECK(end == NULL,           "spt/err");
    CHECK(globerr == ERR_FORMAT, "spt/err_code");
}

/* ================================================================== */
/* setradtimeouts  0x19   YU+W19[AA]+[BB]+[FF]+[DD]                   */
/* ================================================================== */

static void test_setradtimeouts(void)
{
    const char *end;

    /* typical */
    end = parse_w(cmdparser_setradtimeouts, "02+0A+14+07\r");
    CHECK(end != NULL,                                      "srt/valid");
    CHECK(parsed.setradtimeouts.cmdtimeoutmin       == 0x02, "srt/aa");
    CHECK(parsed.setradtimeouts.activewindowmin     == 0x0A, "srt/bb");
    CHECK(parsed.setradtimeouts.passivewindowmin    == 0x14, "srt/ff");
    CHECK(parsed.setradtimeouts.recoverytimeoutdays == 0x07, "srt/dd");

    /* all 0xFF */
    end = parse_w(cmdparser_setradtimeouts, "FF+FF+FF+FF\r");
    CHECK(end != NULL,                                       "srt/FF");
    CHECK(parsed.setradtimeouts.cmdtimeoutmin       == 0xFF, "srt/FF_aa");
    CHECK(parsed.setradtimeouts.recoverytimeoutdays == 0xFF, "srt/FF_dd");

    /* all 0x00 */
    end = parse_w(cmdparser_setradtimeouts, "00+00+00+00\r");
    CHECK(end != NULL, "srt/00");

    /* ERR_FORMAT: missing '+' between first and second field */
    end = parse_w(cmdparser_setradtimeouts, "02X0A+14+07\r");
    CHECK(end == NULL,           "srt/err_plus");
    CHECK(globerr == ERR_FORMAT, "srt/err_plus_code");

    /* ERR_FORMAT: non-hex first field */
    end = parse_w(cmdparser_setradtimeouts, "GG+0A+14+07\r");
    CHECK(end == NULL,           "srt/err_hex");
    CHECK(globerr == ERR_FORMAT, "srt/err_hex_code");
}

/* ================================================================== */
/* setgndtimeouts  0x1A   YU+W1A[NN]                                  */
/* ================================================================== */

static void test_setgndtimeouts(void)
{
    const char *end;

    end = parse_w(cmdparser_setgndtimeouts, "10\r");
    CHECK(end != NULL,                           "sgt/10");
    CHECK(parsed.setgndtimeouts.numcoords == 16, "sgt/10_val");

    end = parse_w(cmdparser_setgndtimeouts, "00\r");
    CHECK(end != NULL,                          "sgt/00");
    CHECK(parsed.setgndtimeouts.numcoords == 0, "sgt/00_val");

    end = parse_w(cmdparser_setgndtimeouts, "FF\r");
    CHECK(end != NULL,                           "sgt/FF");
    CHECK(parsed.setgndtimeouts.numcoords == 255,"sgt/FF_val");

    end = parse_w(cmdparser_setgndtimeouts, "ZZ\r");
    CHECK(end == NULL,           "sgt/err");
    CHECK(globerr == ERR_FORMAT, "sgt/err_code");
}

/* ================================================================== */
/* toggleggb  0x30   YU+W30[E]                                         */
/* ================================================================== */

static void test_toggleggb(void)
{
    const char *end;

    end = parse_w(cmdparser_toggleggb, "0\r");
    CHECK(end != NULL,                  "tggb/disable");
    CHECK(parsed.toggleggb.enable == 0, "tggb/disable_val");

    end = parse_w(cmdparser_toggleggb, "1\r");
    CHECK(end != NULL,                  "tggb/enable");
    CHECK(parsed.toggleggb.enable == 1, "tggb/enable_val");

    /* ERR_FORMAT: value 2 */
    end = parse_w(cmdparser_toggleggb, "2\r");
    CHECK(end == NULL,           "tggb/err_2");
    CHECK(globerr == ERR_FORMAT, "tggb/err_2_code");

    /* ERR_FORMAT: hex letter */
    end = parse_w(cmdparser_toggleggb, "A\r");
    CHECK(end == NULL,           "tggb/err_A");
    CHECK(globerr == ERR_FORMAT, "tggb/err_A_code");
}

/* ================================================================== */
/* ggbextension  0x31                                                   */
/*   R: YU+R31                                                          */
/*   W: YU+W31+[EEEEE]  (unsigned decimal float)                       */
/* ================================================================== */

static void test_ggbextension(void)
{
    const char *end;

    /* read: no args */
    end = parse_r(cmdparser_ggbextension, " \r");
    CHECK(end != NULL, "ggbe/read");

    /* write: typical positive */
    end = parse_w(cmdparser_ggbextension, "+2.50\r");
    CHECK(end != NULL, "ggbe/write");
    CHECK(parsed.ggbextension.extension > 2.49f &&
          parsed.ggbextension.extension < 2.51f, "ggbe/write_val");

    /* write: zero */
    end = parse_w(cmdparser_ggbextension, "+0.00\r");
    CHECK(end != NULL,                           "ggbe/write_zero");
    CHECK(parsed.ggbextension.extension == 0.0f, "ggbe/write_zero_val");

    /* ERR_FORMAT: missing leading '+' */
    end = parse_w(cmdparser_ggbextension, "2.50\r");
    CHECK(end == NULL,           "ggbe/err_noplus");
    CHECK(globerr == ERR_FORMAT, "ggbe/err_noplus_code");

    /* ERR_INV_ARG: negative float after '+' */
    end = parse_w(cmdparser_ggbextension, "+-1.00\r");
    CHECK(end == NULL,            "ggbe/err_neg");
    CHECK(globerr == ERR_INV_ARG, "ggbe/err_neg_code");
}

/* ================================================================== */
/* setggbtarget  0x32   YU+W32[EEEEE]                                  */
/* ================================================================== */

static void test_setggbtarget(void)
{
    const char *end;

    end = parse_w(cmdparser_setggbtarget, "3.75\r");
    CHECK(end != NULL, "sgtgt/valid");
    CHECK(parsed.setggbtarget.target > 3.74f &&
          parsed.setggbtarget.target < 3.76f, "sgtgt/val");

    end = parse_w(cmdparser_setggbtarget, "0.00\r");
    CHECK(end != NULL,                         "sgtgt/zero");
    CHECK(parsed.setggbtarget.target == 0.0f,  "sgtgt/zero_val");

    /* ERR_INV_ARG: negative */
    end = parse_w(cmdparser_setggbtarget, "-1.00\r");
    CHECK(end == NULL,            "sgtgt/err_neg");
    CHECK(globerr == ERR_INV_ARG, "sgtgt/err_neg_code");

    /* ERR_INV_ARG: strtod fails (no numeric chars) */
    end = parse_w(cmdparser_setggbtarget, "\r");
    CHECK(end == NULL,            "sgtgt/err_empty");
    CHECK(globerr == ERR_INV_ARG, "sgtgt/err_empty_code");
}

/* ================================================================== */
/* setggbspeed  0x33   YU+W33[SS]                                      */
/* ================================================================== */

static void test_setggbspeed(void)
{
    const char *end;

    end = parse_w(cmdparser_setggbspeed, "00\r");
    CHECK(end != NULL,                   "sgs/00");
    CHECK(parsed.setggbspeed.speed == 0, "sgs/00_val");

    end = parse_w(cmdparser_setggbspeed, "FF\r");
    CHECK(end != NULL,                      "sgs/FF");
    CHECK(parsed.setggbspeed.speed == 0xFF, "sgs/FF_val");

    end = parse_w(cmdparser_setggbspeed, "7F\r");
    CHECK(end != NULL,                      "sgs/7F");
    CHECK(parsed.setggbspeed.speed == 0x7F, "sgs/7F_val");

    end = parse_w(cmdparser_setggbspeed, "GG\r");
    CHECK(end == NULL,           "sgs/err");
    CHECK(globerr == ERR_FORMAT, "sgs/err_code");
}

/* ================================================================== */
/* togglesunreq  0x34   YU+W34[S]                                      */
/* ================================================================== */

static void test_togglesunreq(void)
{
    const char *end;

    end = parse_w(cmdparser_togglesunreq, "0\r");
    CHECK(end != NULL,                        "tsr/0");
    CHECK(parsed.togglesunreq.requiresun == 0,"tsr/0_val");

    end = parse_w(cmdparser_togglesunreq, "1\r");
    CHECK(end != NULL,                        "tsr/1");
    CHECK(parsed.togglesunreq.requiresun == 1,"tsr/1_val");

    end = parse_w(cmdparser_togglesunreq, "2\r");
    CHECK(end == NULL,           "tsr/err_2");
    CHECK(globerr == ERR_FORMAT, "tsr/err_2_code");

    end = parse_w(cmdparser_togglesunreq, "A\r");
    CHECK(end == NULL,           "tsr/err_A");
    CHECK(globerr == ERR_FORMAT, "tsr/err_A_code");
}

/* ================================================================== */
/* toggledetumb  0x40   YU+W40[E]                                      */
/* ================================================================== */

static void test_toggledetumb(void)
{
    const char *end;

    end = parse_w(cmdparser_toggledetumb, "0\r");
    CHECK(end != NULL,                     "td/disable");
    CHECK(parsed.toggledetumb.enable == 0, "td/disable_val");

    end = parse_w(cmdparser_toggledetumb, "1\r");
    CHECK(end != NULL,                     "td/enable");
    CHECK(parsed.toggledetumb.enable == 1, "td/enable_val");

    end = parse_w(cmdparser_toggledetumb, "2\r");
    CHECK(end == NULL,           "td/err");
    CHECK(globerr == ERR_FORMAT, "td/err_code");
}

/* ================================================================== */
/* setmtqpolar  0x41   YU+W41[P]                                       */
/* ================================================================== */

static void test_setmtqpolar(void)
{
    const char *end;

    end = parse_w(cmdparser_setmtqpolar, "0\r");
    CHECK(end != NULL,                    "smp/normal");
    CHECK(parsed.setmtqpolar.invert == 0, "smp/normal_val");

    end = parse_w(cmdparser_setmtqpolar, "1\r");
    CHECK(end != NULL,                    "smp/invert");
    CHECK(parsed.setmtqpolar.invert == 1, "smp/invert_val");

    end = parse_w(cmdparser_setmtqpolar, "2\r");
    CHECK(end == NULL,           "smp/err");
    CHECK(globerr == ERR_FORMAT, "smp/err_code");
}

/* ================================================================== */
/* setadcsthresh  0x42   YU+W42+[D]+[E]+[F]+[G]+[M]+[N]              */
/*   each field is 4 raw bytes (IEEE 754 float)                        */
/* ================================================================== */

static void test_setadcsthresh(void)
{
    const char *end;
    char buf[64];
    char *p;
    float vals[6];
    int i;

    vals[0] = 0.10f; /* detstopb */
    vals[1] = 0.20f; /* detstopomega */
    vals[2] = 0.30f; /* detresumeb */
    vals[3] = 0.40f; /* detresumeomega */
    vals[4] = 0.50f; /* critb */
    vals[5] = 0.60f; /* critomega */

    p = buf;
    for(i = 0; i < 6; i++) {
        *p++ = '+';
        put_float(p, vals[i]);
        p += 4;
    }
    *p++ = '\r'; *p = '\0';

    end = parse_w(cmdparser_setadcsthresh, buf);
    CHECK(end != NULL, "sat/valid");
    CHECK(parsed.setadcsthresh.detstopb > 0.09f &&
          parsed.setadcsthresh.detstopb < 0.11f,          "sat/detstopb");
    CHECK(parsed.setadcsthresh.detstopomega > 0.19f &&
          parsed.setadcsthresh.detstopomega < 0.21f,      "sat/detstopomega");
    CHECK(parsed.setadcsthresh.detresumeb > 0.29f &&
          parsed.setadcsthresh.detresumeb < 0.31f,        "sat/detresumeb");
    CHECK(parsed.setadcsthresh.detresumeomega > 0.39f &&
          parsed.setadcsthresh.detresumeomega < 0.41f,    "sat/detresumeomega");
    CHECK(parsed.setadcsthresh.critb > 0.49f &&
          parsed.setadcsthresh.critb < 0.51f,             "sat/critb");
    CHECK(parsed.setadcsthresh.critomega > 0.59f &&
          parsed.setadcsthresh.critomega < 0.61f,         "sat/critomega");

    /* zero thresholds */
    p = buf;
    for(i = 0; i < 6; i++) {
        *p++ = '+';
        put_float(p, 0.0f);
        p += 4;
    }
    *p++ = '\r'; *p = '\0';
    end = parse_w(cmdparser_setadcsthresh, buf);
    CHECK(end != NULL,                              "sat/zeros");
    CHECK(parsed.setadcsthresh.detstopb   == 0.0f, "sat/zeros_val");
    CHECK(parsed.setadcsthresh.critomega  == 0.0f, "sat/zeros_last");

    /* ERR_FORMAT: leading '+' missing */
    buf[0] = 'X';
    end = parse_w(cmdparser_setadcsthresh, buf);
    CHECK(end == NULL,           "sat/err_plus0");
    CHECK(globerr == ERR_FORMAT, "sat/err_plus0_code");

    /* ERR_FORMAT: interior '+' missing (byte 5, after first float) */
    p = buf;
    for(i = 0; i < 6; i++) {
        *p++ = '+';
        put_float(p, vals[i]);
        p += 4;
    }
    *p++ = '\r'; *p = '\0';
    buf[5] = 'X'; /* corrupt separator before second float */
    end = parse_w(cmdparser_setadcsthresh, buf);
    CHECK(end == NULL,           "sat/err_plus1");
    CHECK(globerr == ERR_FORMAT, "sat/err_plus1_code");
}

/* ================================================================== */
/* setsunpos  0x43   YU+W43[TTTTTTTTTTTTTTTT]  (16 hex digits)        */
/* ================================================================== */

static void test_setsunpos(void)
{
    const char *end;

    end = parse_w(cmdparser_setsunpos, "0000000060000000\r");
    CHECK(end != NULL, "ssp/valid");
    CHECK(parsed.setsunpos.firstvectime == (uint64_t)0x0000000060000000ULL, "ssp/val");

    end = parse_w(cmdparser_setsunpos, "0000000000000000\r");
    CHECK(end != NULL,                              "ssp/zeros");
    CHECK(parsed.setsunpos.firstvectime == 0ULL,    "ssp/zeros_val");

    end = parse_w(cmdparser_setsunpos, "FFFFFFFFFFFFFFFF\r");
    CHECK(end != NULL, "ssp/FF");
    CHECK(parsed.setsunpos.firstvectime == (uint64_t)0xFFFFFFFFFFFFFFFFULL, "ssp/FF_val");

    /* ERR_FORMAT: invalid hex char */
    end = parse_w(cmdparser_setsunpos, "GGGGGGGGGGGGGGGG\r");
    CHECK(end == NULL,           "ssp/err_hex");
    CHECK(globerr == ERR_FORMAT, "ssp/err_hex_code");

    /* ERR_FORMAT: only 15 valid hex chars then CR */
    end = parse_w(cmdparser_setsunpos, "000000006000000\r");
    CHECK(end == NULL,           "ssp/err_short");
    CHECK(globerr == ERR_FORMAT, "ssp/err_short_code");
}

/* ================================================================== */
/* settle  0x44   YU+W44  (no inline args)                            */
/* ================================================================== */

static void test_settle(void)
{
    const char *end;

    end = parse_w(cmdparser_settle, " \r");
    CHECK(end != NULL, "tle/space");

    end = parse_w(cmdparser_settle, "\r");
    CHECK(end != NULL, "tle/nospace");
}

/* ================================================================== */
/* all no-argument read commands: 0x45 0x46 0x47 0x48 0x49 0x4B       */
/* ================================================================== */

static void test_noarg_reads(void)
{
    const char *end;

    end = parse_r(cmdparser_getattmat,   " \r"); CHECK(end != NULL, "r45/space");
    end = parse_r(cmdparser_getattmat,   "\r");  CHECK(end != NULL, "r45/nospace");
    end = parse_r(cmdparser_getomega,    " \r"); CHECK(end != NULL, "r46/space");
    end = parse_r(cmdparser_getomega,    "\r");  CHECK(end != NULL, "r46/nospace");
    end = parse_r(cmdparser_getpos,      " \r"); CHECK(end != NULL, "r47/space");
    end = parse_r(cmdparser_getpos,      "\r");  CHECK(end != NULL, "r47/nospace");
    end = parse_r(cmdparser_getphase,    " \r"); CHECK(end != NULL, "r48/space");
    end = parse_r(cmdparser_getphase,    "\r");  CHECK(end != NULL, "r48/nospace");
    end = parse_r(cmdparser_getbdot,     " \r"); CHECK(end != NULL, "r49/space");
    end = parse_r(cmdparser_getbdot,     "\r");  CHECK(end != NULL, "r49/nospace");
    end = parse_r(cmdparser_getbodyvecs, " \r"); CHECK(end != NULL, "r4B/space");
    end = parse_r(cmdparser_getbodyvecs, "\r");  CHECK(end != NULL, "r4B/nospace");
}

/* ================================================================== */
/* setmtqtime  0x4E   YU+W4E+[TTTT];[+XX];[+YY];[+ZZ]                */
/* ================================================================== */

static void test_setmtqtime(void)
{
    const char *end;

    /* minimum valid: duration=1, all components zero */
    end = parse_w(cmdparser_setmtqtime, "+0001;+00;+00;+00\r");
    CHECK(end != NULL,                        "mtq/min");
    CHECK(parsed.setmtqtime.durationsec == 1, "mtq/min_dur");
    CHECK(parsed.setmtqtime.xpct        == 0, "mtq/min_x");
    CHECK(parsed.setmtqtime.ypct        == 0, "mtq/min_y");
    CHECK(parsed.setmtqtime.zpct        == 0, "mtq/min_z");

    /* max duration, max +100 components */
    end = parse_w(cmdparser_setmtqtime, "+FFFF;+64;+64;+64\r");
    CHECK(end != NULL,                           "mtq/max");
    CHECK(parsed.setmtqtime.durationsec == 0xFFFF,"mtq/max_dur");
    CHECK(parsed.setmtqtime.xpct        == 100,  "mtq/max_x");
    CHECK(parsed.setmtqtime.ypct        == 100,  "mtq/max_y");
    CHECK(parsed.setmtqtime.zpct        == 100,  "mtq/max_z");

    /* min -100 components */
    end = parse_w(cmdparser_setmtqtime, "+0001;-64;-64;-64\r");
    CHECK(end != NULL,                       "mtq/min_neg");
    CHECK(parsed.setmtqtime.xpct == -100,    "mtq/min_neg_x");
    CHECK(parsed.setmtqtime.ypct == -100,    "mtq/min_neg_y");
    CHECK(parsed.setmtqtime.zpct == -100,    "mtq/min_neg_z");

    /* mixed signs: x=-100, y=-50, z=0 */
    end = parse_w(cmdparser_setmtqtime, "+0010;-64;-32;+00\r");
    CHECK(end != NULL,               "mtq/mixed");
    CHECK(parsed.setmtqtime.xpct == -100, "mtq/mixed_x");
    CHECK(parsed.setmtqtime.ypct == -50,  "mtq/mixed_y");
    CHECK(parsed.setmtqtime.zpct == 0,    "mtq/mixed_z");

    /* ERR_FORMAT: missing initial '+' before duration */
    end = parse_w(cmdparser_setmtqtime, "0001;+00;+00;+00\r");
    CHECK(end == NULL,           "mtq/err_noplus");
    CHECK(globerr == ERR_FORMAT, "mtq/err_noplus_code");

    /* ERR_INV_ARG: duration = 0 */
    end = parse_w(cmdparser_setmtqtime, "+0000;+00;+00;+00\r");
    CHECK(end == NULL,            "mtq/err_dur0");
    CHECK(globerr == ERR_INV_ARG, "mtq/err_dur0_code");

    /* ERR_FORMAT: non-hex in duration */
    end = parse_w(cmdparser_setmtqtime, "+GGGG;+00;+00;+00\r");
    CHECK(end == NULL,           "mtq/err_dur_hex");
    CHECK(globerr == ERR_FORMAT, "mtq/err_dur_hex_code");

    /* ERR_FORMAT: missing ';' before first component */
    end = parse_w(cmdparser_setmtqtime, "+0001X+00;+00;+00\r");
    CHECK(end == NULL,           "mtq/err_semi");
    CHECK(globerr == ERR_FORMAT, "mtq/err_semi_code");

    /* ERR_FORMAT: missing sign on component */
    end = parse_w(cmdparser_setmtqtime, "+0001;00;+00;+00\r");
    CHECK(end == NULL,           "mtq/err_sign");
    CHECK(globerr == ERR_FORMAT, "mtq/err_sign_code");

    /* ERR_INV_ARG: x component = 101 (0x65) */
    end = parse_w(cmdparser_setmtqtime, "+0001;+65;+00;+00\r");
    CHECK(end == NULL,            "mtq/err_over");
    CHECK(globerr == ERR_INV_ARG, "mtq/err_over_code");

    /* ERR_INV_ARG: x component = -101 (-0x65) */
    end = parse_w(cmdparser_setmtqtime, "+0001;-65;+00;+00\r");
    CHECK(end == NULL,            "mtq/err_under");
    CHECK(globerr == ERR_INV_ARG, "mtq/err_under_code");

    /* ERR_FORMAT: non-hex in component value */
    end = parse_w(cmdparser_setmtqtime, "+0001;+GG;+00;+00\r");
    CHECK(end == NULL,           "mtq/err_comp_hex");
    CHECK(globerr == ERR_FORMAT, "mtq/err_comp_hex_code");
}

/* ================================================================== */
/* setpulsbins  0x51   YU+W51[DAC output data]  (8 raw bytes)         */
/* ================================================================== */

static void test_setpulsbins(void)
{
    const char *end;
    char buf[16];
    int i;

    /* incrementing pattern */
    for(i = 0; i < 8; i++)
        buf[i] = (char)(i * 0x11);
    buf[8] = '\r'; buf[9] = '\0';
    end = parse_w(cmdparser_setpulsbins, buf);
    CHECK(end != NULL, "pb/pattern");
    for(i = 0; i < 8; i++)
        CHECK((uint8_t)parsed.setpulsbins.dacoutputs[i] == (uint8_t)(i * 0x11),
              "pb/pattern_val");

    /* all zeros */
    memset(buf, 0, 8);
    buf[8] = '\r'; buf[9] = '\0';
    end = parse_w(cmdparser_setpulsbins, buf);
    CHECK(end != NULL, "pb/zeros");
    for(i = 0; i < 8; i++)
        CHECK(parsed.setpulsbins.dacoutputs[i] == 0, "pb/zeros_val");

    /* all 0xFF */
    memset(buf, (int)(unsigned char)0xFF, 8);
    buf[8] = '\r'; buf[9] = '\0';
    end = parse_w(cmdparser_setpulsbins, buf);
    CHECK(end != NULL, "pb/FF");
    for(i = 0; i < 8; i++)
        CHECK(parsed.setpulsbins.dacoutputs[i] == 0xFF, "pb/FF_val");
}

/* ================================================================== */
/* main                                                                 */
/* ================================================================== */

int main(void)
{
    test_recitebeacon();
    test_setpowerbounds();
    test_linkspeed();
    test_setcpuspeed();
    test_obctime();
    test_useresetcounts();
    test_logstatus();
    test_logsinrange();
    test_clearlogrange();
    test_setpasstimes();
    test_setradtimeouts();
    test_setgndtimeouts();
    test_toggleggb();
    test_ggbextension();
    test_setggbtarget();
    test_setggbspeed();
    test_togglesunreq();
    test_setsunpos();
    test_toggledetumb();
    test_setmtqpolar();
    test_setadcsthresh();
    test_settle();
    test_noarg_reads();
    test_setmtqtime();
    test_setpulsbins();

    printf("\n=== Results: %d passed, %d failed ===\n", pass_count, fail_count);
    return fail_count ? 1 : 0;
}
