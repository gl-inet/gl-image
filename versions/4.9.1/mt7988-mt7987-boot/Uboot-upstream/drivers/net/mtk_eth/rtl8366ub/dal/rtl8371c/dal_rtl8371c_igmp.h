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
 * Purpose : RTL8367/RTL8371C switch high-level API
 *
 * Feature : The file includes IGMP module high-layer API defination
 *
 */

#ifndef __DAL_rtl8371C_IGMP_H__
#define __DAL_rtl8371C_IGMP_H__

#include "../../igmp.h"

/* Function Name:
 *      dal_rtl8371c_igmp_protocol_set
 * Description:
 *      set IGMP/MLD protocol action
 * Input:
 *      unit        - Unit ID
 *      port        - Port ID
 *      protocol    - IGMP/MLD protocol
 *      action      - Per-port and per-protocol IGMP action seeting
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_MASK       - Error parameter
 * Note:
 *      This API set IGMP/MLD protocol action
 */
extern rtksw_api_ret_t dal_rtl8371c_igmp_protocol_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_igmp_protocol_t protocol, rtksw_igmp_action_t action);

/* Function Name:
 *      dal_rtl8371c_igmp_protocol_get
 * Description:
 *      set IGMP/MLD protocol action
 * Input:
 *      unit        - Unit ID
 *      port        - Port ID
 *      protocol    - IGMP/MLD protocol
 *      action      - Per-port and per-protocol IGMP action seeting
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_MASK       - Error parameter
 * Note:
 *      This API set IGMP/MLD protocol action
 */
extern rtksw_api_ret_t dal_rtl8371c_igmp_protocol_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_igmp_protocol_t protocol, rtksw_igmp_action_t *pAction);

/* Function Name:
 *      dal_rtl8371c_igmp_bypassGroupRange_set
 * Description:
 *      Set Bypass group
 * Input:
 *      unit        - Unit ID
 *      group       - bypassed group
 *      enabled     - enabled 1: Bypassed, 0: not bypass
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT           - Error Input
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_igmp_bypassGroupRange_set(rtksw_uint32 unit, rtksw_igmp_bypassGroup_t group, rtksw_enable_t enabled);

/* Function Name:
 *      dal_rtl8371c_igmp_bypassGroupRange_get
 * Description:
 *      get Bypass group
 * Input:
 *      unit        - Unit ID
 *      group       - bypassed group
 * Output:
 *      pEnable     - enabled 1: Bypassed, 0: not bypass
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT           - Error Input
 *      RT_ERR_NULL_POINTER    - Null Pointer
 * Note:
 *
 */
extern rtksw_api_ret_t dal_rtl8371c_igmp_bypassGroupRange_get(rtksw_uint32 unit, rtksw_igmp_bypassGroup_t group, rtksw_enable_t *pEnable);

#endif /* __DAL_rtl8371C_IGMP_H__ */
