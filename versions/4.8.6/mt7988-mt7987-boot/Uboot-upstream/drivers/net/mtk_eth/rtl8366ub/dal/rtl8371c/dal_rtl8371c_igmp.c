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
 * $Revision$
 * $Date$
 *
 * Purpose : RTK switch high-level API for RTL8367/RTL8367C
 * Feature : Here is a list of all functions and variables in IGMP module.
 *
 */

#include "../../rtk_switch.h"
#include "../../rtk_error.h"
#include "dal_rtl8371c_igmp.h"
#include "../reg.h"
#include "rtk_rtl8371c_reg_struct.h"

#include "../dal_mgmts.h"

#define RTL8371C_PROTOCOL_OP_FLOOD   1
#define RTL8371C_PROTOCOL_OP_TRAP    2
#define RTL8371C_PROTOCOL_OP_DROP    3

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
rtksw_api_ret_t dal_rtl8371c_igmp_protocol_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_igmp_protocol_t protocol, rtksw_igmp_action_t action)
{
    rtksw_uint32      operation;
    rtksw_api_ret_t   retVal;
    rtksw_uint32        phy_port;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    /* Check port valid */
    RTKSW_CHK_PORT_VALID(unit, port);

    if(protocol >= PROTOCOL_END)
        return RT_ERR_INPUT;

    if(action >= IGMP_ACTION_END)
        return RT_ERR_INPUT;

    switch(action)
    {
        case IGMP_ACTION_FORWARD:
            operation = RTL8371C_PROTOCOL_OP_FLOOD;
            break;
        case IGMP_ACTION_TRAP2CPU:
            operation = RTL8371C_PROTOCOL_OP_TRAP;
            break;
        case IGMP_ACTION_DROP:
            operation = RTL8371C_PROTOCOL_OP_DROP;
            break;
        case IGMP_ACTION_ASIC:
        default:
            return RT_ERR_INPUT;
    }

    phy_port = rtksw_switch_port_L2P_get(unit, port);

    switch(protocol)
    {
        case PROTOCOL_IGMPv1:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_IGMPV1_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_IGMPv2:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_IGMPV2_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_IGMPv3:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_IGMPV3_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_MLDv1:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_MLDV1_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_MLDv2:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_MLDV2_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        default:
            return RT_ERR_INPUT;

    }

    return RT_ERR_OK;
}

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
rtksw_api_ret_t dal_rtl8371c_igmp_protocol_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_igmp_protocol_t protocol, rtksw_igmp_action_t *pAction)
{
    rtksw_uint32      operation;
    rtksw_api_ret_t   retVal;
    rtksw_uint32        phy_port;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    /* Check port valid */
    RTKSW_CHK_PORT_VALID(unit, port);

    if(protocol >= PROTOCOL_END)
        return RT_ERR_INPUT;

    if(pAction == NULL)
        return RT_ERR_NULL_POINTER;

    phy_port = rtksw_switch_port_L2P_get(unit, port);

    switch(protocol)
    {
        case PROTOCOL_IGMPv1:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_IGMPV1_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_IGMPv2:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_IGMPV2_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_IGMPv3:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_IGMPV3_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_MLDv1:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_MLDV1_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        case PROTOCOL_MLDv2:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_PORT_CFGr, phy_port, REG_ARRAY_INDEX_NONE, RTL8371C_MLDV2_OPf, &operation)) != RT_ERR_OK)
                return retVal;
            break;
        default:
            return RT_ERR_INPUT;

    }

    switch(operation)
    {
        case RTL8371C_PROTOCOL_OP_FLOOD:
            *pAction = IGMP_ACTION_FORWARD;
            break;
        case RTL8371C_PROTOCOL_OP_TRAP:
            *pAction = IGMP_ACTION_TRAP2CPU;
            break;
        case RTL8371C_PROTOCOL_OP_DROP:
            *pAction = IGMP_ACTION_DROP;
            break;
        default:
            return RT_ERR_FAILED;
    }

    return RT_ERR_OK;
}

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
rtksw_api_ret_t dal_rtl8371c_igmp_bypassGroupRange_set(rtksw_uint32 unit, rtksw_igmp_bypassGroup_t group, rtksw_enable_t enabled)
{
    rtksw_api_ret_t   retVal;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(group >= IGMP_BYPASS_GROUP_END)
        return RT_ERR_INPUT;

    if(enabled >= RTKSW_ENABLE_END)
        return RT_ERR_INPUT;

    switch (group)
    {
        case IGMP_BYPASS_224_0_0_X:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP4_BYPASS_224_0_0f, (rtksw_uint32 *)&enabled)) != RT_ERR_OK)
                return retVal;
            break;
        case IGMP_BYPASS_224_0_1_X:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP4_BYPASS_224_0_1f, (rtksw_uint32 *)&enabled)) != RT_ERR_OK)
                return retVal;
            break;
        case IGMP_BYPASS_239_255_255_X:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP4_BYPASS_239_255_255f, (rtksw_uint32 *)&enabled)) != RT_ERR_OK)
                return retVal;
            break;
        case IGMP_BYPASS_IPV6_00XX:
            if ((retVal = reg16_array_field_write(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP6_BYPASSf, (rtksw_uint32 *)&enabled)) != RT_ERR_OK)
                return retVal;
            break;
        default:
            return RT_ERR_INPUT;
    }

    return RT_ERR_OK;
}

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
rtksw_api_ret_t dal_rtl8371c_igmp_bypassGroupRange_get(rtksw_uint32 unit, rtksw_igmp_bypassGroup_t group, rtksw_enable_t *pEnable)
{
    rtksw_api_ret_t   retVal;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(group >= IGMP_BYPASS_GROUP_END)
        return RT_ERR_INPUT;

    if(NULL == pEnable)
        return RT_ERR_NULL_POINTER;

    switch (group)
    {
        case IGMP_BYPASS_224_0_0_X:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP4_BYPASS_224_0_0f,(rtksw_uint32 *)pEnable)) != RT_ERR_OK)
                return retVal;
            break;
        case IGMP_BYPASS_224_0_1_X:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP4_BYPASS_224_0_1f,(rtksw_uint32 *)pEnable)) != RT_ERR_OK)
                return retVal;
            break;
        case IGMP_BYPASS_239_255_255_X:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP4_BYPASS_239_255_255f,(rtksw_uint32 *)pEnable)) != RT_ERR_OK)
                return retVal;
            break;
        case IGMP_BYPASS_IPV6_00XX:
            if ((retVal = reg16_array_field_read(unit, RTL8371C_IGMP_CTRLr, REG_ARRAY_INDEX_NONE, REG_ARRAY_INDEX_NONE, RTL8371C_IGMP_MLD_IP6_BYPASSf,(rtksw_uint32 *)pEnable)) != RT_ERR_OK)
                return retVal;
            break;
        default:
            return RT_ERR_INPUT;
    }

    return RT_ERR_OK;
}

