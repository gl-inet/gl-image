/*
 * Copyright (C) 2024 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * $Revision$
 * $Date$
 *
 * Purpose : RTK switch Wake on LAN high-level API
 * Feature : Here is a list of all functions and variables in wol module.
 *
 */

#ifndef __RTKSW_API_WOL_H__
#define __RTKSW_API_WOL_H__

/* Function Name:
 *      rtksw_wol_init
 * Description:
 *      Initial WoL function.
 * Input:
 *      unit        - Unit ID
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 * Note:
 *      This API initialize the WoL function.
 *
 *      After initialization, rtksw_wol_enable_set can be used to turn on or turn off WoL function.
 */
extern rtksw_api_ret_t rtksw_wol_init(rtksw_uint32 unit);

/* Function Name:
 *      rtksw_wol_state_set
 * Description:
 *      Enable/Disable WoL function.
 * Input:
 *      unit        - Unit ID
 *      state       - The state of WoL function
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 * Note:
 *      This API enable/disable the WoL function.
 */
extern rtksw_api_ret_t rtksw_wol_state_set(rtksw_uint32 unit, rtksw_enable_t state);

/* Function Name:
 *      rtksw_wol_systemMac_set
 * Description:
 *      Set WOL system MAC address.
 * Input:
 *      unit        - Unit ID
 *      pMac        - System MAC
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_INPUT        - Error Input
 *      RT_ERR_SMI          - SMI access error
 * Note:
 *      This API configure the MAC address of WoL function.
 */
extern rtksw_api_ret_t rtksw_wol_systemMac_set(rtksw_uint32 unit, rtksw_mac_t *pMac);

#endif /* __RTKSW_API_WOL_H__ */
