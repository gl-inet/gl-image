/*
 * Copyright (C) 2019 Realtek Semiconductor Corp.
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
 * Purpose : RTK switch high-level API for RTL8367/RTL8367D
 * Feature : Here is a list of all functions and variables in GPIO module.
 *
 */

#include "../../rtk_switch.h"
#include "../../rtk_error.h"
#include "dal_rtl8371c_gpio.h"
#include "../reg.h"
#include "rtk_rtl8371c_reg_struct.h"

#include "../dal_mgmts.h"

/* Function Name:
 *      dal_rtl8371c_gpio_input_get
 * Description:
 *      Get gpio input
 * Input:
 *      unit        - Unit ID
 *      pin         - GPIO pin
 * Output:
 *      pInput         - GPIO input
 * Return:
 *      RT_ERR_OK                 - Success
 *      RT_ERR_SMI              - SMI access error
 *      RT_ERR_OUT_OF_RANGE     - input out of range.
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer.
 * Note:
 *      None
 */
rtksw_api_ret_t dal_rtl8371c_gpio_input_get(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_uint32 *pInput)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if(NULL == pInput)
    {
        return RT_ERR_NULL_POINTER;
    }
    
    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_INPUT0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    pin_offset = pin % 16;

    *pInput = (regData&(1<<pin_offset))>>pin_offset;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_output_set
 * Description:
 *      Set GPIO output value.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 *      output  - 1 or 0
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK            - OK
 *      RT_ERR_FAILED        - Failed
 *      RT_ERR_SMI           - SMI access error
 *      RT_ERR_INPUT         - Invalid input parameter.
 *      RT_ERR_OUT_OF_RANGE  - input parameter out of range.
 * Note:
 *      The API can set GPIO pin output 1 or 0.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_output_set(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_uint32 output)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if (output > 1)
    {
        return RT_ERR_INPUT;
    }
    
    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_OUTPUT0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    pin_offset = pin % 16;

    if (output!=0)
    {
        regData = regData|(1<<pin_offset);
    }
    else
    {
        regData = regData&(~(1<<pin_offset));
    }

    if ((retVal = reg16_write(unit, RTL8371C_CFG_GPIO_OUTPUT0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_output_get
 * Description:
 *      Get GPIO output.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 * Output:
 *      pOutput  - GPIO output
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 *      RT_ERR_SMI              - SMI access error
 *      RT_ERR_INPUT            - Invalid input parameters.
 *      RT_ERR_OUT_OF_RANGE     - input parameter out of range.
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer.
 * Note:
 *      The API can get GPIO output.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_output_get(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_uint32 *pOutput)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if(NULL == pOutput)
    {
        return RT_ERR_NULL_POINTER;
    }
    
    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_OUTPUT0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    pin_offset = pin % 16;

    *pOutput = (regData&(1<<pin_offset))>>pin_offset;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_state_set
 * Description:
 *      Set GPIO control.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 *      state   - GPIO enable
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK            - OK
 *      RT_ERR_FAILED        - Failed
 *      RT_ERR_SMI           - SMI access error
 *      RT_ERR_INPUT         - Invalid input parameter.
 *      RT_ERR_OUT_OF_RANGE  - input parameter out of range.
 *      RT_ERR_ENABLE        - invalid enable parameter .
 * Note:
 *      The API can set GPIO state.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_state_set(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_enable_t state)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 pin_offset;
    rtksw_uint32 regData;
    rtksw_uint32 gpioRegAddr[2] = {RTL8371C_GPIO_MODE_X0r, RTL8371C_GPIO_MODE_X1r};

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin > RTL8371C_GPIOPINMAX)
        return RT_ERR_OUT_OF_RANGE;

    if ((retVal = reg16_read(unit, gpioRegAddr[pin / 16], &regData)) != RT_ERR_OK)
        return retVal;

    pin_offset = pin % 16;
    if (state == RTKSW_ENABLED)
        regData = regData | (1<<pin_offset);
    else
        regData = regData & (~(1<<pin_offset));

    if ((retVal = reg16_write(unit, gpioRegAddr[pin / 16], &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_state_get
 * Description:
 *      Get GPIO enable state.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 * Output:
 *      pState   - GPIO enable
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 *      RT_ERR_SMI              - SMI access error
 *      RT_ERR_INPUT            - Invalid input parameters.
 *      RT_ERR_OUT_OF_RANGE     - input parameter out of range.
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer.
 * Note:
 *      The API can get GPIO state.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_state_get(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_enable_t *pState)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 pin_offset;
    rtksw_uint32 regData;
    rtksw_uint32 gpioRegAddr[2] = {RTL8371C_GPIO_MODE_X0r, RTL8371C_GPIO_MODE_X1r};

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin > RTL8371C_GPIOPINMAX)
        return RT_ERR_OUT_OF_RANGE;

    if(NULL == pState)
        return RT_ERR_NULL_POINTER;
        
    if ((retVal = reg16_read(unit, gpioRegAddr[pin / 16], &regData)) != RT_ERR_OK)
        return retVal;

    pin_offset = pin % 16;
    if (regData & (0x0001 << pin_offset))
        *pState = RTKSW_ENABLED;
    else
        *pState = RTKSW_DISABLED;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_mode_set
 * Description:
 *      Set GPIO mode.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 *      mode    - 1 or 0
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK            - OK
 *      RT_ERR_FAILED        - Failed
 *      RT_ERR_SMI           - SMI access error
 *      RT_ERR_INPUT         - Invalid input parameter.
 *      RT_ERR_OUT_OF_RANGE  - input parameter out of range.
 * Note:
 *      The API can set GPIO  to input or output mode.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_mode_set(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_gpio_mode_t mode)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;    
    
    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    pin_offset = pin % 16;

    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_OUTPUT_EN0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    switch (mode)
    {
        case GPIO_MODE_OUTPUT:
            regData = regData|(1<<pin_offset);
            break;
        case GPIO_MODE_INPUT:
            regData = regData&(~(1<<pin_offset));
            break;
        default:
            return RT_ERR_INPUT;
    }

    if ((retVal = reg16_write(unit, RTL8371C_CFG_GPIO_OUTPUT_EN0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_mode_get
 * Description:
 *      Get GPIO mode.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 * Output:
 *      pMode    - GPIO mode
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 *      RT_ERR_SMI              - SMI access error
 *      RT_ERR_INPUT            - Invalid input parameters.
 *      RT_ERR_OUT_OF_RANGE     - input parameter out of range.
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer.
 * Note:
 *      The API can get GPIO mode.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_mode_get(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_gpio_mode_t *pMode)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;    
    rtksw_uint32 gpioMode;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if(NULL == pMode)
        return RT_ERR_NULL_POINTER;

    pin_offset = pin % 16;

    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_OUTPUT_EN0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    gpioMode = (regData&(1<<pin_offset)>>pin_offset);

    switch (gpioMode)
    {
        case 0:
            *pMode = GPIO_MODE_INPUT;
            break;
        case 1:
            *pMode = GPIO_MODE_OUTPUT;
            break;
        default:
            return RT_ERR_FAILED;
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_source_set
 * Description:
 *      Set GPIO mode.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 *      mode    - GPIO source mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK            - OK
 *      RT_ERR_FAILED        - Failed
 *      RT_ERR_SMI           - SMI access error
 *      RT_ERR_INPUT         - Invalid input parameter.
 *      RT_ERR_OUT_OF_RANGE  - input parameter out of range.
 * Note:
 *      The API can set GPIO source mode.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_source_set(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_gpio_source_t mode)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;    
    
    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    pin_offset = pin % 16;

    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_SOURCE0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    switch (mode)
    {
        case GPIO_SOURCE_CPU:
            regData = regData|(1<<pin_offset);
            break;
        case GPIO_SOURCE_8051:
            regData = regData&(~(1<<pin_offset));
            break;
        default:
            return RT_ERR_INPUT;
    }

    if ((retVal = reg16_write(unit, RTL8371C_CFG_GPIO_OUTPUT_EN0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_source_get
 * Description:
 *      Get GPIO mode.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 * Output:
 *      pMode    - GPIO source mode
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 *      RT_ERR_SMI              - SMI access error
 *      RT_ERR_INPUT            - Invalid input parameters.
 *      RT_ERR_OUT_OF_RANGE     - input parameter out of range.
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer.
 * Note:
 *      The API can get GPIO source mode.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_source_get(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_gpio_source_t *pMode)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData, pin_offset;    
    rtksw_uint32 gpioMode;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if(NULL == pMode)
        return RT_ERR_NULL_POINTER;

    pin_offset = pin % 16;

    if ((retVal = reg16_read(unit, RTL8371C_CFG_GPIO_SOURCE0r + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    gpioMode = (regData&(1<<pin_offset)>>pin_offset);

    switch (gpioMode)
    {
        case 0:
            *pMode = GPIO_SOURCE_CPU;
            break;
        case 1:
            *pMode = GPIO_SOURCE_8051;
            break;
        default:
            return RT_ERR_FAILED;
    }

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8371c_gpio_aclEnClear_set
 * Description:
 *      Set GPIO acl clear.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK            - OK
 *      RT_ERR_FAILED        - Failed
 *      RT_ERR_SMI           - SMI access error
 *      RT_ERR_INPUT         - Invalid input parameter.
 *      RT_ERR_OUT_OF_RANGE  - input parameter out of range.
 * Note:
 *      The API can set GPIO ACL clear.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_aclEnClear_set(rtksw_uint32 unit, rtksw_uint32 pin)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if ((retVal = reg16_read(unit, RTL8371C_ACL_GPIO_CFGr + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;
    
    regData |= (1 << (pin % 16));
    
    /* ACL clear */
    if ((retVal = reg16_write(unit, RTL8371C_ACL_GPIO_CFGr + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8371c_gpio_aclEnClear_get
 * Description:
 *      Get GPIO acl clear.
 * Input:
 *      unit    - Unit ID
 *      pin     - GPIO pin
 * Output:
 *      pAclEn   - GPIO acl enable
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 *      RT_ERR_SMI              - SMI access error
 *      RT_ERR_INPUT            - Invalid input parameters.
 *      RT_ERR_OUT_OF_RANGE     - input parameter out of range.
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer.
 * Note:
 *      The API can get GPIO acl enable clear.
 */
rtksw_api_ret_t dal_rtl8371c_gpio_aclEnClear_get(rtksw_uint32 unit, rtksw_uint32 pin, rtksw_enable_t *pAclEn)
{
    rtksw_api_ret_t retVal;
    rtksw_uint32 regData;

    /* Check initialization state */
    RTKSW_CHK_INIT_STATE(unit);

    if(pin >= RTL8371C_GPIOPINNO)
        return RT_ERR_OUT_OF_RANGE;

    if(NULL == pAclEn)
        return RT_ERR_NULL_POINTER;

    if ((retVal = reg16_read(unit, RTL8371C_ACL_GPIO_CFGr + (pin / 16), &regData)) != RT_ERR_OK)
        return retVal;

    *pAclEn = (regData & (1 << (pin % 16))) ? RTKSW_ENABLED : RTKSW_DISABLED;
    return RT_ERR_OK;
}


