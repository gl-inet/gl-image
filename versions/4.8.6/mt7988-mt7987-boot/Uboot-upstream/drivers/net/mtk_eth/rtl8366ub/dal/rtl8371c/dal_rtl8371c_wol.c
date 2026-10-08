#include "../reg.h"
#include "rtk_rtl8371c_reg_struct.h"

#include "dal_rtl8371c_wol.h"
#include "../../rtk_error.h"

#include "../dal_mgmts.h"

/* Function Name:
 *      dal_rtl8371c_wol_init
 * Description:
 *      Initial WoL function.
 * Input:
 *      None
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
rtksw_api_ret_t dal_rtl8371c_wol_init(rtksw_uint32 unit)
{
    /* Skip Warning */
    if (unit) {}

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_wol_state_set
 * Description:
 *      Enable/Disable WoL function.
 * Input:
 *      state   - The state of WoL function
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 * Note:
 *      This API enable/disable the WoL function.
 */
rtksw_api_ret_t dal_rtl8371c_wol_state_set(rtksw_uint32 unit, rtksw_enable_t state)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData;

    if(state >= RTKSW_ENABLE_END)
        return RT_ERR_INPUT;

    regData = (state == RTKSW_ENABLED) ? 1 : 0;
    if ((retVal = reg16_field_write(unit, RTL8371C_WOL_CTRLr, RTL8371C_WOL_ENf, &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_wol_systemMac_set
 * Description:
 *      Set WOL system MAC address.
 * Input:
 *      pMac   - System MAC
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
rtksw_api_ret_t dal_rtl8371c_wol_systemMac_set(rtksw_uint32 unit, rtksw_mac_t *pMac)
{
    rtksw_uint32      regData;
    rtksw_api_ret_t   retVal;

    if(pMac == NULL)
        return RT_ERR_INPUT;

    regData = ((pMac->octet[0] << 8) | pMac->octet[1]);
    if ((retVal = reg16_field_write(unit, RTL8371C_WOL_MACr, RTL8371C_MAC_47_32f, &regData)) != RT_ERR_OK)
        return retVal;

    regData = ((pMac->octet[2] << 8) | pMac->octet[3]);
    if ((retVal = reg16_field_write(unit, RTL8371C_WOL_MACr, RTL8371C_MAC_31_16f, &regData)) != RT_ERR_OK)
        return retVal;

    regData = ((pMac->octet[4] << 8) | pMac->octet[5]);
    if ((retVal = reg16_field_write(unit, RTL8371C_WOL_MACr, RTL8371C_MAC_15_0f, &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

