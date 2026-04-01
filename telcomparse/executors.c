#include "executors.h"
#include "parsers.h"

#include <inttypes.h>
#include <stdio.h>

int cmdexecutor_recitebeacon(void)
{
    /* no inline args */
    return 0;
}

int cmdexecutor_setpowerbounds(void)
{
    setpowerbounds_t *info;

    info = (void*) &parsed;
    printf("    which: %d\n", info->which);
    printf("    level: %d\n", info->level);
    printf("    duration: %d\n", info->duration);
    return 0;
}

int cmdexecutor_setpowerthresh(void)
{
    /* spec says TODO */
    return 0;
}

int cmdexecutor_settempthresh(void)
{
    /* no inline args */
    return 0;
}

int cmdexecutor_radioack(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_downlinkreq(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_linkspeed(void)
{
    linkspeed_t *info;

    info = (void*) &parsed;
    printf("    rfmode: %02"PRIX8"\n", info->rfmode);
    return 0;
}

int cmdexecutor_setcpuspeed(void)
{
    setcpuspeed_t *info;

    info = (void*) &parsed;
    printf("    freqmhz: %"PRIu8"\n", info->freqmhz);
    return 0;
}

int cmdexecutor_obctime(void)
{
    obctime_t *info;

    info = (void*) &parsed;
    if(cmdtype == CMDTYPE_WRITE)
        printf("    timestamp: %016"PRIX64"\n", info->timestamp);
    return 0;
}

int cmdexecutor_useresetcounts(void)
{
    /* no inline args */
    return 0;
}

int cmdexecutor_logstatus(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_logsinrange(void)
{
    logsinrange_t *info;

    info = (void*) &parsed;
    printf("    logtype: %02"PRIX8"\n", info->logtype);
    printf("    starttime: %016"PRIX64"\n", info->starttime);
    printf("    endtime: %016"PRIX64"\n", info->endtime);
    return 0;
}

int cmdexecutor_clearlogrange(void)
{
    clearlogrange_t *info;

    info = (void*) &parsed;
    printf("    logtype: %02"PRIX8"\n", info->logtype);
    printf("    starttime: %016"PRIX64"\n", info->starttime);
    printf("    endtime: %016"PRIX64"\n", info->endtime);
    return 0;
}

int cmdexecutor_setpasstimes(void)
{
    setpasstimes_t *info;

    info = (void*) &parsed;
    printf("    numpasses: %"PRIu8"\n", info->numpasses);
    return 0;
}

int cmdexecutor_setradtimeouts(void)
{
    setradtimeouts_t *info;

    info = (void*) &parsed;
    printf("    cmdtimeoutmin: %"PRIu8"\n", info->cmdtimeoutmin);
    printf("    activewindowmin: %"PRIu8"\n", info->activewindowmin);
    printf("    passivewindowmin: %"PRIu8"\n", info->passivewindowmin);
    printf("    recoverytimeoutdays: %"PRIu8"\n", info->recoverytimeoutdays);
    return 0;
}

int cmdexecutor_setgndtimeouts(void)
{
    setgndtimeouts_t *info;

    info = (void*) &parsed;
    printf("    numcoords: %"PRIu8"\n", info->numcoords);
    return 0;
}

int cmdexecutor_toggleggb(void)
{
    toggleggb_t *info;

    info = (void*) &parsed;
    printf("    enable: %d\n", info->enable);
    return 0;
}

int cmdexecutor_ggbextension(void)
{
    ggbextension_t *info;

    info = (void*) &parsed;
    if(cmdtype == CMDTYPE_WRITE)
        printf("    extension: %g\n", (double)info->extension);
    return 0;
}

int cmdexecutor_setggbtarget(void)
{
    setggbtarget_t *info;

    info = (void*) &parsed;
    printf("    target: %g\n", (double)info->target);
    return 0;
}

int cmdexecutor_setggbspeed(void)
{
    setggbspeed_t *info;

    info = (void*) &parsed;
    printf("    speed: %02"PRIX8"\n", info->speed);
    return 0;
}

int cmdexecutor_togglesunreq(void)
{
    togglesunreq_t *info;

    info = (void*) &parsed;
    printf("    requiresun: %d\n", info->requiresun);
    return 0;
}

int cmdexecutor_setsunpos(void)
{
    setsunpos_t *info;

    info = (void*) &parsed;
    printf("    firstvectime: %016"PRIX64"\n", info->firstvectime);
    return 0;
}

int cmdexecutor_toggledetumb(void)
{
    toggledetumb_t *info;

    info = (void*) &parsed;
    printf("    enable: %d\n", info->enable);
    return 0;
}

int cmdexecutor_setmtqpolar(void)
{
    setmtqpolar_t *info;

    info = (void*) &parsed;
    printf("    invert: %d\n", info->invert);
    return 0;
}

int cmdexecutor_setadcsthresh(void)
{
    setadcsthresh_t *info;

    info = (void*) &parsed;
    printf("    detstopb: %g\n",       (double)info->detstopb);
    printf("    detstopomega: %g\n",   (double)info->detstopomega);
    printf("    detresumeb: %g\n",     (double)info->detresumeb);
    printf("    detresumeomega: %g\n", (double)info->detresumeomega);
    printf("    critb: %g\n",          (double)info->critb);
    printf("    critomega: %g\n",      (double)info->critomega);
    return 0;
}

int cmdexecutor_settle(void)
{
    /* no inline args */
    return 0;
}

int cmdexecutor_getattmat(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_getomega(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_getpos(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_getphase(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_getbdot(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_getbodyvecs(void)
{
    /* no args */
    return 0;
}

int cmdexecutor_setmtqtime(void)
{
    setmtqtime_t *info;

    info = (void*) &parsed;
    printf("    durationsec: %"PRIu16"\n", info->durationsec);
    printf("    xpct: %d\n", (int)info->xpct);
    printf("    ypct: %d\n", (int)info->ypct);
    printf("    zpct: %d\n", (int)info->zpct);
    return 0;
}

int cmdexecutor_setpulsbins(void)
{
    setpulsbins_t *info;
    int i;

    info = (void*) &parsed;
    for(i=0; i<8; i++)
        printf("    dacoutputs[%d]: %02"PRIX8"\n", i, info->dacoutputs[i]);
    return 0;
}
