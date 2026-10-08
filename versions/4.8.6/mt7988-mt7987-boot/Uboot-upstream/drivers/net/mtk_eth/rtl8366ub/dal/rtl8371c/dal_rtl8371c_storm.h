   /*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * Purpose : RTL8367/RTL8367C switch high-level API
 *
 * Feature : The file includes Storm module high-layer API defination
 *
 */

#ifndef __DAL_RTL8371C_STORM_H__
#define __DAL_RTL8371C_STORM_H__

#include "../../storm.h"

/* Function Name:
 *      dal_rtl8371c_rate_stormControlMeterIdx_set
 * Description:
 *      Set the storm control meter index.
 *  Input:
 *      unit        - Unit ID
 *      port        - port id
 *      storm_type  - storm group type
 *      index       - storm control meter index.
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID - Invalid port id
 *      RT_ERR_FILTER_METER_ID  - Invalid meter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlMeterIdx_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_rate_storm_group_t stormType, rtksw_uint32 index);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlMeterIdx_get
 * Description:
 *      Get the storm control meter index.
 *  Input:
 *      unit        - Unit ID
 *      port        - port id
 *      storm_type  - storm group type
 * Output:
 *      pIndex     - storm control meter index.
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID - Invalid port id
 *      RT_ERR_FILTER_METER_ID  - Invalid meter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlMeterIdx_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_rate_storm_group_t stormType, rtksw_uint32 *pIndex);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlPortEnable_set
 * Description:
 *      Set enable status of storm control on specified port.
 *  Input:
 *      unit        - Unit ID
 *      port        - port id
 *      stormType   - storm group type
 *      enable      - enable status of storm control
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_PORT_ID           - invalid port id
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlPortEnable_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_rate_storm_group_t stormType, rtksw_enable_t enable);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlPortEnable_set
 * Description:
 *      Set enable status of storm control on specified port.
 *  Input:
 *      unit        - Unit ID
 *      port        - port id
 *      stormType   - storm group type
 * Output:
 *      pEnable     - enable status of storm control
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_PORT_ID           - invalid port id
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlPortEnable_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_rate_storm_group_t stormType, rtksw_enable_t *pEnable);

/* Function Name:
 *      dal_rtl8371c_storm_bypass_set
 * Description:
 *      Set bypass storm filter control configuration.
 *  Input:
 *      unit        - Unit ID
 *      type        - Bypass storm filter control type.
 *      enable      - Bypass status.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 *      RT_ERR_ENABLE       - Invalid IFG parameter
 * Note:
 *
 *      This API can set per-port bypass stomr filter control frame type including RMA and igmp.
 *      The bypass frame type is as following:
 *      - BYPASS_BRG_GROUP,
 *      - BYPASS_FD_PAUSE,
 *      - BYPASS_SP_MCAST,
 *      - BYPASS_1X_PAE,
 *      - BYPASS_UNDEF_BRG_04,
 *      - BYPASS_UNDEF_BRG_05,
 *      - BYPASS_UNDEF_BRG_06,
 *      - BYPASS_UNDEF_BRG_07,
 *      - BYPASS_PROVIDER_BRIDGE_GROUP_ADDRESS,
 *      - BYPASS_UNDEF_BRG_09,
 *      - BYPASS_UNDEF_BRG_0A,
 *      - BYPASS_UNDEF_BRG_0B,
 *      - BYPASS_UNDEF_BRG_0C,
 *      - BYPASS_PROVIDER_BRIDGE_GVRP_ADDRESS,
 *      - BYPASS_8021AB,
 *      - BYPASS_UNDEF_BRG_0F,
 *      - BYPASS_BRG_MNGEMENT,
 *      - BYPASS_UNDEFINED_11,
 *      - BYPASS_UNDEFINED_12,
 *      - BYPASS_UNDEFINED_13,
 *      - BYPASS_UNDEFINED_14,
 *      - BYPASS_UNDEFINED_15,
 *      - BYPASS_UNDEFINED_16,
 *      - BYPASS_UNDEFINED_17,
 *      - BYPASS_UNDEFINED_18,
 *      - BYPASS_UNDEFINED_19,
 *      - BYPASS_UNDEFINED_1A,
 *      - BYPASS_UNDEFINED_1B,
 *      - BYPASS_UNDEFINED_1C,
 *      - BYPASS_UNDEFINED_1D,
 *      - BYPASS_UNDEFINED_1E,
 *      - BYPASS_UNDEFINED_1F,
 *      - BYPASS_GMRP,
 *      - BYPASS_GVRP,
 *      - BYPASS_UNDEF_GARP_22,
 *      - BYPASS_UNDEF_GARP_23,
 *      - BYPASS_UNDEF_GARP_24,
 *      - BYPASS_UNDEF_GARP_25,
 *      - BYPASS_UNDEF_GARP_26,
 *      - BYPASS_UNDEF_GARP_27,
 *      - BYPASS_UNDEF_GARP_28,
 *      - BYPASS_UNDEF_GARP_29,
 *      - BYPASS_UNDEF_GARP_2A,
 *      - BYPASS_UNDEF_GARP_2B,
 *      - BYPASS_UNDEF_GARP_2C,
 *      - BYPASS_UNDEF_GARP_2D,
 *      - BYPASS_UNDEF_GARP_2E,
 *      - BYPASS_UNDEF_GARP_2F,
 *      - BYPASS_IGMP.
 */
extern rtksw_api_ret_t dal_rtl8371c_storm_bypass_set(rtksw_uint32 unit, rtksw_storm_bypass_t type, rtksw_enable_t enable);

/* Function Name:
 *      dal_rtl8371c_storm_bypass_get
 * Description:
 *      Get bypass storm filter control configuration.
 *  Input:
 *      unit        - Unit ID
 *      type        - Bypass storm filter control type.
 * Output:
 *      pEnable - Bypass status.
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API can get per-port bypass stomr filter control frame type including RMA and igmp.
 *      The bypass frame type is as following:
 *      - BYPASS_BRG_GROUP,
 *      - BYPASS_FD_PAUSE,
 *      - BYPASS_SP_MCAST,
 *      - BYPASS_1X_PAE,
 *      - BYPASS_UNDEF_BRG_04,
 *      - BYPASS_UNDEF_BRG_05,
 *      - BYPASS_UNDEF_BRG_06,
 *      - BYPASS_UNDEF_BRG_07,
 *      - BYPASS_PROVIDER_BRIDGE_GROUP_ADDRESS,
 *      - BYPASS_UNDEF_BRG_09,
 *      - BYPASS_UNDEF_BRG_0A,
 *      - BYPASS_UNDEF_BRG_0B,
 *      - BYPASS_UNDEF_BRG_0C,
 *      - BYPASS_PROVIDER_BRIDGE_GVRP_ADDRESS,
 *      - BYPASS_8021AB,
 *      - BYPASS_UNDEF_BRG_0F,
 *      - BYPASS_BRG_MNGEMENT,
 *      - BYPASS_UNDEFINED_11,
 *      - BYPASS_UNDEFINED_12,
 *      - BYPASS_UNDEFINED_13,
 *      - BYPASS_UNDEFINED_14,
 *      - BYPASS_UNDEFINED_15,
 *      - BYPASS_UNDEFINED_16,
 *      - BYPASS_UNDEFINED_17,
 *      - BYPASS_UNDEFINED_18,
 *      - BYPASS_UNDEFINED_19,
 *      - BYPASS_UNDEFINED_1A,
 *      - BYPASS_UNDEFINED_1B,
 *      - BYPASS_UNDEFINED_1C,
 *      - BYPASS_UNDEFINED_1D,
 *      - BYPASS_UNDEFINED_1E,
 *      - BYPASS_UNDEFINED_1F,
 *      - BYPASS_GMRP,
 *      - BYPASS_GVRP,
 *      - BYPASS_UNDEF_GARP_22,
 *      - BYPASS_UNDEF_GARP_23,
 *      - BYPASS_UNDEF_GARP_24,
 *      - BYPASS_UNDEF_GARP_25,
 *      - BYPASS_UNDEF_GARP_26,
 *      - BYPASS_UNDEF_GARP_27,
 *      - BYPASS_UNDEF_GARP_28,
 *      - BYPASS_UNDEF_GARP_29,
 *      - BYPASS_UNDEF_GARP_2A,
 *      - BYPASS_UNDEF_GARP_2B,
 *      - BYPASS_UNDEF_GARP_2C,
 *      - BYPASS_UNDEF_GARP_2D,
 *      - BYPASS_UNDEF_GARP_2E,
 *      - BYPASS_UNDEF_GARP_2F,
 *      - BYPASS_IGMP.
 */
extern rtksw_api_ret_t dal_rtl8371c_storm_bypass_get(rtksw_uint32 unit, rtksw_storm_bypass_t type, rtksw_enable_t *pEnable);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlExtPortmask_set
 * Description:
 *      Set externsion storm control port mask
 *  Input:
 *      unit        - Unit ID
 *      pPortmask   - port mask
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlExtPortmask_set(rtksw_uint32 unit, rtksw_portmask_t *pPortmask);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlExtPortmask_get
 * Description:
 *      Set externsion storm control port mask
 *  Input:
 *      unit        - Unit ID
 * Output:
 *      pPortmask  - port mask
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlExtPortmask_get(rtksw_uint32 unit, rtksw_portmask_t *pPortmask);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlExtEnable_set
 * Description:
 *      Set externsion storm control state
 *  Input:
 *      unit        - Unit ID
 *      stormType   - storm group type
 *      enable      - externsion storm control state
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlExtEnable_set(rtksw_uint32 unit, rtksw_rate_storm_group_t stormType, rtksw_enable_t enable);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlExtEnable_get
 * Description:
 *      Get externsion storm control state
 *  Input:
 *      unit        - Unit ID
 *      stormType   - storm group type
 * Output:
 *      pEnable     - externsion storm control state
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlExtEnable_get(rtksw_uint32 unit, rtksw_rate_storm_group_t stormType, rtksw_enable_t *pEnable);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlExtMeterIdx_set
 * Description:
 *      Set externsion storm control meter index
 *  Input:
 *      unit        - Unit ID
 *      stormType   - storm group type
 *      index       - externsion storm control state
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlExtMeterIdx_set(rtksw_uint32 unit, rtksw_rate_storm_group_t stormType, rtksw_uint32 index);

/* Function Name:
 *      dal_rtl8371c_rate_stormControlExtMeterIdx_get
 * Description:
 *      Get externsion storm control meter index
 *  Input:
 *      unit        - Unit ID
 *      stormType   - storm group type
 *      pIndex      - externsion storm control state
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_NOT_INIT          - The module is not initial
 *      RT_ERR_INPUT             - invalid input parameter
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_rate_stormControlExtMeterIdx_get(rtksw_uint32 unit, rtksw_rate_storm_group_t stormType, rtksw_uint32 *pIndex);

#endif /* __DAL_RTL8371C_STORM_H__ */


