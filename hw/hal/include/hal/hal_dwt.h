#ifndef __CIONIC_DWT_HAL_H__
#define __CIONIC_DWT_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int hal_dwt_cyccnt_init(void);
int hal_dwt_cyccnt_start(void);
int hal_dwt_cyccnt_stop(void);
int hal_dwt_cyccnt_reset(void);
uint64_t hal_dwt_cyccnt_get(void);
uint32_t hal_dwt_cyccnt_get_us(void);

#ifdef __cplusplus
}
#endif

#endif /* __CIONIC_DWT_HAL_H__ */
