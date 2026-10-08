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
 * $Revision$
 * $Date$
 *
 * Purpose : RTL8367/RTL8367C switch high-level API
 *
 * Feature : The file includes the following modules and sub-modules
 *           (1) OAM (802.3ah) configuration
 *
 */

#ifndef __RTKSW_OAM_H__
#define __RTKSW_OAM_H__

/*
 * Symbol Definition
 */


/*
 * Data Declaration
 */


/*
 * Macro Declaration
 */

typedef enum rtksw_oam_parser_act_e
{
    OAM_PARSER_ACTION_FORWARD = 0,
    OAM_PARSER_ACTION_LOOPBACK,
    OAM_PARSER_ACTION_DISCARD,
    OAM_PARSER_ACTION_END,

} rtksw_oam_parser_act_t;

typedef enum rtksw_oam_multiplexer_act_e
{
    OAM_MULTIPLEXER_ACTION_FORWARD = 0,
    OAM_MULTIPLEXER_ACTION_DISCARD,
    OAM_MULTIPLEXER_ACTION_CPUONLY,
    OAM_MULTIPLEXER_ACTION_END,

} rtksw_oam_multiplexer_act_t;


/*
 * Function Declaration
 */

/* Function Name:
 *      rtksw_oam_init
 * Description:
 *      Initialize oam module.
 * Input:
 *      unit     - Unit ID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      Must initialize oam module before calling any oam APIs.
 */
extern rtksw_api_ret_t rtksw_oam_init(rtksw_uint32 unit);

/* Function Name:
 *      rtksw_oam_state_set
 * Description:
 *      This API set OAM state.
 * Input:
 *      unit     - Unit ID
 *      enabled  - OAM state
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT           - Error parameter
 * Note:
 *      This API set OAM state.
 */
extern rtksw_api_ret_t rtksw_oam_state_set(rtksw_uint32 unit, rtksw_enable_t enabled);

/* Function Name:
 *      rtksw_oam_state_get
 * Description:
 *      This API get OAM state.
 * Input:
 *      unit     - Unit ID
 * Output:
 *      pEnabled        - OAM state
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT           - Error parameter
 * Note:
 *      This API set current OAM state.
 */
extern rtksw_api_ret_t rtksw_oam_state_get(rtksw_uint32 unit, rtksw_enable_t *pEnabled);

/* Function Name:
 *      rtksw_oam_parserAction_set
 * Description:
 *      Set OAM parser action
 * Input:
 *      unit    - Unit ID
 *      port    - port id
 *      action  - parser action
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
extern rtksw_api_ret_t  rtksw_oam_parserAction_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_oam_parser_act_t action);

/* Function Name:
 *      rtksw_oam_parserAction_set
 * Description:
 *      Get OAM parser action
 * Input:
 *      unit    - Unit ID
 *      port    - port id
 * Output:
 *      pAction  - parser action
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
extern rtksw_api_ret_t  rtksw_oam_parserAction_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_oam_parser_act_t *pAction);

/* Function Name:
 *      rtksw_oam_multiplexerAction_set
 * Description:
 *      Set OAM multiplexer action
 * Input:
 *      unit    - Unit ID
 *      port    - port id
 *      action  - parser action
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
extern rtksw_api_ret_t  rtksw_oam_multiplexerAction_set(rtksw_uint32 unit, rtksw_port_t port, rtksw_oam_multiplexer_act_t action);

/* Function Name:
 *      rtksw_oam_parserAction_set
 * Description:
 *      Get OAM multiplexer action
 * Input:
 *      unit    - Unit ID
 *      port    - port id
 * Output:
 *      pAction  - parser action
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
extern rtksw_api_ret_t  rtksw_oam_multiplexerAction_get(rtksw_uint32 unit, rtksw_port_t port, rtksw_oam_multiplexer_act_t *pAction);

#if defined(CONFIG_COMPATIBLE_V1_4)

#define rtk_oam_init(void)                              rtksw_oam_init(0)
#define rtk_oam_state_set(enabled)                      rtksw_oam_state_set(0, enabled)
#define rtk_oam_state_get(pEnabled)                     rtksw_oam_state_get(0, pEnabled)
#define rtk_oam_parserAction_set(port, action)          rtksw_oam_parserAction_set(0, port, action)
#define rtk_oam_parserAction_get(port, pAction)         rtksw_oam_parserAction_get(0, port, pAction)
#define rtk_oam_multiplexerAction_set(port, action)     rtksw_oam_multiplexerAction_set(0, port, action)
#define rtk_oam_multiplexerAction_get(port, pAction)    rtksw_oam_multiplexerAction_get(0, port, pAction)

#define rtk_oam_parser_act_t        rtksw_oam_parser_act_t
#define rtk_oam_multiplexer_act_t   rtksw_oam_multiplexer_act_t

#endif /* CONFIG_COMPATIBLE_V1_4 */

#endif /* __RTKSW_OAM_H__ */

