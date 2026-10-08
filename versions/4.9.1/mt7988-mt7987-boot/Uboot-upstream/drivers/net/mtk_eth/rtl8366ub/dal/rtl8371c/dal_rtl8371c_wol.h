

#ifndef __DAL_RTL8371C_WOL_H__
#define __DAL_RTL8371C_WOL_H__

/*
 * Include Files
 */
#include "../../rtk_types.h"
#include "../../rtk_error.h"

/* Function Name:
 *      dal_rtl8371c_wol_init
 * Description:
 *      Initial WoL function.
 *  Input:
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
rtksw_api_ret_t dal_rtl8371c_wol_init(rtksw_uint32 unit);

/* Function Name:
 *      dal_rtl8371c_wol_state_set
 * Description:
 *      Enable/Disable WoL function.
 *  Input:
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
rtksw_api_ret_t dal_rtl8371c_wol_state_set(rtksw_uint32 unit, rtksw_enable_t state);

/* Function Name:
 *      dal_rtl8371c_wol_systemMac_set
 * Description:
 *      Set WOL system MAC address.
 *  Input:
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
rtksw_api_ret_t dal_rtl8371c_wol_systemMac_set(rtksw_uint32 unit, rtksw_mac_t *pMac);

#endif /* __DAL_RTL8371C_WOL_H__ */

