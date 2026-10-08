/*
 * Copyright(c) Realtek Semiconductor Corporation, 2008
 * All rights reserved.
 *
 * $Revision$
 * $Date$
 *
 * Purpose : Definition function prototype of RTK switch API.
 *
 * Feature : Function prototype definition
 *
 */

#ifndef __CHIP_H__
#define __CHIP_H__


#include "rtk_types.h"

#define UNDEFINE_PHY_PORT   (0xFF)
#define RTKSW_SWITCH_PORT_NUM (32)

typedef enum switch_chip_e
{
    CHIP_RTL8367C = 0,
    CHIP_RTL8370B,
    CHIP_RTL8364B,
    CHIP_RTL8363SC_VB,
    CHIP_RTL8367D,
    CHIP_RTL8370C,
    CHIP_RTL8367E,
    CHIP_RTL8371C,
    CHIP_END
}switch_chip_t;

typedef enum port_type_e
{
    UTP_PORT = 0,
    EXT_PORT,
    UNKNOWN_PORT = 0xFF,
    PORT_TYPE_END
}port_type_t;

typedef struct rtksw_switch_halCtrl_s
{
    switch_chip_t   switch_type;
    rtksw_uint32      l2p_port[RTKSW_SWITCH_PORT_NUM];
    rtksw_uint32      p2l_port[RTKSW_SWITCH_PORT_NUM];
    port_type_t     log_port_type[RTKSW_SWITCH_PORT_NUM];
    rtksw_uint32      ptp_port[RTKSW_SWITCH_PORT_NUM];
    rtksw_uint32      valid_portmask;
    rtksw_uint32      valid_utp_portmask;
    rtksw_uint32      valid_ext_portmask;
    rtksw_uint32      valid_cpu_portmask;
    rtksw_uint32      min_phy_port;
    rtksw_uint32      max_phy_port;
    rtksw_uint32      phy_portmask;
    rtksw_uint32      combo_logical_port;
    rtksw_uint32      usxg_logical_portmask;
    rtksw_uint32      hsg_logical_portmask;
    rtksw_uint32      sg_logical_portmask;
    rtksw_uint32      utp_2p5g_logical_portmask;
    rtksw_uint32      max_meter_id;
    rtksw_uint32      max_lut_addr_num;
    rtksw_uint32      trunk_group_mask;
    rtksw_uint32      packet_buffer_page_num;

}rtksw_switch_halCtrl_t;

/* Function Name:
 *      switch_probe
 * Description:
 *      Probe switch
 * Input:
 *      unit            - Unit ID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK       - Switch probed
 *      RT_ERR_FAILED   - Switch Unprobed.
 * Note:
 *
 */
extern rtksw_api_ret_t switch_probe(rtksw_uint32 unit, switch_chip_t *pSwitchChip);

/* Function Name:
 *      rtksw_hal_find_device
 * Description:
 *      Find the mac chip from SDK supported mac device lists.
 * Input:
 *      unit        - Unit ID
 * Output:
 *      None
 * Return:
 *      NULL        - Not found
 *      Otherwise   - Pointer of mac chip HAL structure that found
 * Note:
 *
 */
extern rtksw_switch_halCtrl_t *rtksw_hal_find_device(rtksw_uint32 unit);

#endif /* End of __CHIP_H__ */
