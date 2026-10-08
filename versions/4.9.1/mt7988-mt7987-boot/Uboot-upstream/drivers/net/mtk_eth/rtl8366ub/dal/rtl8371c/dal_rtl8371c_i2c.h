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
 * Feature : The file includes I2C module high-layer API defination
 *
 */


#ifndef __DAL_rtl8371C_I2C_H__
#define __DAL_rtl8371C_I2C_H__

#include "../../rtk_types.h"
#include "../../i2c.h"

#define I2C_GPIO_MAX_GROUP (3)

/* Function Name:
 *      dal_rtl8371c_i2c_init
 * Description:
 *      I2C smart function initialization.
 * Input:
 *      unit    - Unit ID
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 * Note:
 *      This API is used to initialize EEE status.
 *      need used GPIO pins
 *      OpenDrain and clock
 */
extern rtksw_api_ret_t dal_rtl8371c_i2c_init(rtksw_uint32 unit);

/* Function Name:
 *      dal_rtl8371c_i2c_data_read
 * Description:
 *      read i2c slave device register.
 * Input:
 *      unit            - Unit ID
 *      deviceAddr      - access Slave device address
 *      slaveRegAddr    - access Slave register address
 * Output:
 *      pRegData     -   read data
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer
 * Note:
 *      The API can access i2c slave and read i2c slave device register.
 */
extern rtksw_api_ret_t dal_rtl8371c_i2c_data_read(rtksw_uint32 unit, rtksw_uint8 deviceAddr, rtksw_uint32 slaveRegAddr, rtksw_uint32 *pRegData);

/* Function Name:
 *      dal_rtl8371c_i2c_data_write
 * Description:
 *      write data to i2c slave device register
 * Input:
 *      unit            - Unit ID
 *      deviceAddr      - access Slave device address
 *      slaveRegAddr    - access Slave register address
 *      regData         - data to set
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK               - OK
 * Note:
 *      The API can access i2c slave and setting i2c slave device register.
 */
extern rtksw_api_ret_t dal_rtl8371c_i2c_data_write(rtksw_uint32 unit, rtksw_uint8 deviceAddr, rtksw_uint32 slaveRegAddr, rtksw_uint32 regData);

/* Function Name:
 *      dal_rtl8371c_i2c_mode_set
 * Description:
 *      Set I2C data byte-order.
 * Input:
 *      unit    - Unit ID
 *      i2cmode - byte-order mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_INPUT        - Invalid input parameter.
 * Note:
 *      This API can set I2c traffic's byte-order .
 */
extern rtksw_api_ret_t dal_rtl8371c_i2c_mode_set(rtksw_uint32 unit, rtksw_I2C_16bit_mode_t i2cmode);

/* Function Name:
 *      dal_rtl8371c_i2c_mode_get
 * Description:
 *      Get i2c traffic byte-order setting.
 * Input:
 *      unit    - Unit ID
 * Output:
 *      pI2cMode - i2c byte-order
 * Return:
 *      RT_ERR_OK               - OK
 *      RT_ERR_FAILED           - Failed
 *      RT_ERR_NULL_POINTER     - input parameter is null pointer
 * Note:
 *      The API can get i2c traffic byte-order setting.
 */
extern rtksw_api_ret_t dal_rtl8371c_i2c_mode_get(rtksw_uint32 unit, rtksw_I2C_16bit_mode_t *pI2cMode);

#endif /* end of __DAL_rtl8371C_I2C_H__ */

