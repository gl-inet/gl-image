
/*
 * Copyright (C) 2011 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * Purpose : Use to Management each device
 *
 * Feature : The file have include the following module and sub-modules
 *           1) Initialize system
 *           2) Initialize device
 *           3) Mangement Devices
 *
 */
#ifndef __DAL_MGMT_H__
#define __DAL_MGMT_H__

/*
 * Include Files
 */
#include "../rtk_types.h"
#include "../rtk_error.h"
#include "dal_mappers.h"
#if defined(CONFIG_MSSDK)
#include <common/rt_autoconf.h>
#endif
/*
 * Symbol Definition
 */
#if defined(RTK_X86_CLE)
#define DAL_MGMT_MAX_UNIT   4
#elif defined(CONFIG_MSSDK)
#define  DAL_MGMT_MAX_UNIT  (CONFIG_SDK_MAX_UNIT_MS + CONFIG_SDK_MAX_UNIT_US)
#else
#define DAL_MGMT_MAX_UNIT   1
#endif

typedef struct dal_mgmt_info_s
{
    dal_mapper_t    *pMapper[DAL_MGMT_MAX_UNIT];
} dal_mgmt_info_t;

typedef struct dal_mapper_info_s
{
    switch_chip_t       switchChip;
    dal_mapper_t        *pMapper;
} dal_mapper_info_t;

/*
 * Data Declaration
 */
extern dal_mgmt_info_t      *pRtkswMgmt_node;

/*
 * Macro Definition
 */
#define RT_MGMT       pRtkswMgmt_node
#define RT_MAPPER     RT_MGMT->pMapper[unit]


/*
 * Function Declaration
 */

/* Module Name : */

/* Function Name:
 *      dal_mgmt_attachDevice
 * Description:
 *      Attach device(semaphore, database clear)
 * Input:
 *      unit            - unit ID
 *      switchChip      - switch type
 * Output:
 *      None
 * Return:
 *      RT_ERR_FAILED   - initialize fail
 *      RT_ERR_OK       - initialize success
 * Note:
 *      RTK must call this function before do other kind of action.
 */
extern rtksw_int32
dal_mgmt_attachDevice(rtksw_uint32 unit, switch_chip_t switchChip);

#endif /* __DAL_MGMT_H__ */


