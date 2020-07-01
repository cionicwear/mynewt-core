
#include "os/mynewt.h"
#include "nrf52840.h"
#include "hal/hal_dwt.h"

#define MAX_CYCCNT_VALUE    (4294967294)
static uint64_t g_tot_ccnt = 0;
static uint64_t g_prev_ccnt = 0;

int hal_dwt_cyccnt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    g_tot_ccnt = 0;
    g_prev_ccnt = 0;
    return 0;
}

int hal_dwt_cyccnt_start(void)
{
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    return 0;
}

int hal_dwt_cyccnt_stop(void)
{
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;

    return 0;
}

int hal_dwt_cyccnt_reset(void)
{
    DWT->CYCCNT = 0;
    g_tot_ccnt = 0;
    g_prev_ccnt = 0;
    return 0;
}

uint64_t hal_dwt_cyccnt_get(void)
{
    // if counter wrapped
    if(g_prev_ccnt > DWT->CYCCNT){
        g_tot_ccnt += (MAX_CYCCNT_VALUE - g_prev_ccnt) + DWT->CYCCNT;
    }else{
        g_tot_ccnt += (DWT->CYCCNT - g_prev_ccnt);
    }

    g_prev_ccnt = DWT->CYCCNT;

    return (uint64_t)g_tot_ccnt;
}