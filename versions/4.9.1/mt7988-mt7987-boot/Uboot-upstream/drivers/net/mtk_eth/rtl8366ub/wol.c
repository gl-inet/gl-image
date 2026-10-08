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

#include "rtk_error.h"
#include "rtk_types.h"

#include "wol.h"
#include "dal/dal_mgmts.h"

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
rtksw_api_ret_t rtksw_wol_init(rtksw_uint32 unit)
{
    rtksw_api_ret_t retVal;

    RTKSW_CHK_UNIT_ID(unit);
    RTKSW_CHK_RT_MAPPER(unit);
    
    if (NULL == RT_MAPPER->wol_init)
        return RT_ERR_DRIVER_NOT_FOUND;

    RTKSW_API_LOCK(unit);
    retVal = RT_MAPPER->wol_init(unit);
    RTKSW_API_UNLOCK(unit);

    return retVal;
}

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
rtksw_api_ret_t rtksw_wol_state_set(rtksw_uint32 unit, rtksw_enable_t state)
{
    rtksw_api_ret_t retVal;

    RTKSW_CHK_UNIT_ID(unit);
    RTKSW_CHK_RT_MAPPER(unit);
    
    if (NULL == RT_MAPPER->wol_state_set)
        return RT_ERR_DRIVER_NOT_FOUND;

    RTKSW_API_LOCK(unit);
    retVal = RT_MAPPER->wol_state_set(unit, state);
    RTKSW_API_UNLOCK(unit);

    return retVal;
}

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
rtksw_api_ret_t rtksw_wol_systemMac_set(rtksw_uint32 unit, rtksw_mac_t *pMac)
{
    rtksw_api_ret_t retVal;

    RTKSW_CHK_UNIT_ID(unit);
    RTKSW_CHK_RT_MAPPER(unit);
    
    if (NULL == RT_MAPPER->wol_systemMac_set)
        return RT_ERR_DRIVER_NOT_FOUND;

    RTKSW_API_LOCK(unit);
    retVal = RT_MAPPER->wol_systemMac_set(unit, pMac);
    RTKSW_API_UNLOCK(unit);

    return retVal;
}

