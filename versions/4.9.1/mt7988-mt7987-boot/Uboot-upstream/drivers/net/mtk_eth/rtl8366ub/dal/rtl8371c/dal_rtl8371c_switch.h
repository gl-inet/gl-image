
/*
 * Copyright (C) 2012 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * Purpose : Definition of Switch Global API
 *
 * Feature : The file have include the following module and sub-modules
 *           (1) Switch parameter settings
 *
 */

#ifndef __DAL_RTL8371C_SWITCH_H__
#define __DAL_RTL8371C_SWITCH_H__

/*
 * Include Files
 */
#include "../../rtk_types.h"
#include "../../rtk_switch.h"

/*
 * Symbol Definition
 */

/*
 * Data Declaration
 */

/*
 * Function Declaration
 */

/* Module Name    : Switch     */
/* Sub-module Name: Switch parameter settings */

/* Function Name:
 *      dal_rtl8371c_switch_init
 * Description:
 *      Initialize switch module of the specified device.
 *  Input:
 *      unit        - Unit ID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      Module must be initialized before using all of APIs in this module
 */
extern rtksw_api_ret_t dal_rtl8371c_switch_init(rtksw_uint32 unit);

/* Function Name:
 *      dal_rtl8371c_switch_mac_set
 * Description:
 *      Set switch MAC address.
 *  Input:
 *      unit        - Unit ID
 *      pMac        - Switch MAC
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_INPUT        - Error Input
 *      RT_ERR_SMI          - SMI access error
 * Note:
 *      This API configure the MAC address of switch function.
 */
extern rtksw_api_ret_t dal_rtl8371c_switch_mac_set(rtksw_uint32 unit, rtksw_mac_t *pMac);

/* Function Name:
 *      dal_rtl8371c_switch_mac_get
 * Description:
 *      Get switch MAC address
 *  Input:
 *      unit        - Unit ID
 *      pMac        - switch mac
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK       - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      This API get the MAC address of switch function.
 */
extern rtksw_api_ret_t dal_rtl8371c_switch_mac_get(rtksw_uint32 unit, rtksw_mac_t *pMac);

/* Function Name:
 *      dal_rtl8371c_switch_reset
 * Description:
 *      Reset chip 
 * Input:
 *      unit    - Unit ID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 * Note:
 *      The API can reset entire chip.
 */
extern rtksw_api_ret_t dal_rtl8371c_switch_reset(rtksw_uint32 unit);

#endif /* __DAL_RTL8371C_SWITCH_H__ */

