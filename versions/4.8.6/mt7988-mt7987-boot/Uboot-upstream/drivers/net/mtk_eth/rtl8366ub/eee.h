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
 * Feature : The file includes EEE module high-layer API defination
 *
 */

#ifndef __RTKSW_API_EEE_H__
#define __RTKSW_API_EEE_H__

/* Function Name:
 *      rtksw_eee_init
 * Description:
 *      EEE function initialization.
 * Input:
 *      unit    - Unit ID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 * Note:
 *      This API is used to initialize EEE status.
 */
extern rtksw_api_ret_t rtksw_eee_init(rtksw_uint32 unit);

/* Function Name:
 *      rtksw_eee_portEnable_set
 * Description:
 *      Set enable status of EEE function.
 * Input:
 *      unit    - Unit ID
 *      port    - port id.
 *      enable  - enable EEE status.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 *      RT_ERR_ENABLE - Invalid enable input.
 * Note:
 *      This API can set EEE function to the specific port.
 *      The configuration of the port is as following:
 *      - DISABLE
 *      - ENABLE
 */
extern rtksw_api_ret_t rtksw_eee_portEnable_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_enable_t enable);

/* Function Name:
 *      rtksw_eee_portEnable_get
 * Description:
 *      Get port admin configuration of the specific port.
 * Input:
 *      unit    - Unit ID
 *      port    - Port id.
 * Output:
 *      pEnable - Back pressure status.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_PORT_ID - Invalid port number.
 * Note:
 *      This API can set EEE function to the specific port.
 *      The configuration of the port is as following:
 *      - DISABLE
 *      - ENABLE
 */
extern rtksw_api_ret_t rtksw_eee_portEnable_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_enable_t *pEnable);

#if defined(CONFIG_COMPATIBLE_V1_4)

#define rtk_eee_init(void)                      rtksw_eee_init(0)
#define rtk_eee_portEnable_set(port, enable)    rtksw_eee_portEnable_set(0, port, enable)
#define rtk_eee_portEnable_get(port, pEnable)   rtksw_eee_portEnable_get(0, port, pEnable)

#endif /* CONFIG_COMPATIBLE_V1_4 */

#endif /* __RTKSW_API_EEE_H__ */

