
#include "../reg.h"
#include "rtk_rtl8371c_reg_struct.h"
#include "rtk_rtl8371c_table_struct.h"
#include "rtk_rtl8371c_macdrv.h"

static rtksw_macPpInfo_t rtl8371c_macPpInfo =
{
    0x1FA0, /* lowerbound_addr */
    0x23A0, /* upperbound_addr */
    0x80,  /* interval */
};

static rtksw_macPpInfo_t rtl8371c_macPTPPpInfo =
{
    0x3DB0, /* lowerbound_addr */
    0x3E30, /* upperbound_addr */
    0x10,  /* interval */
};

/* RTL8371C mac driver service APIs */
rtksw_macdrv_t rtl8371c_macdrv =
{
    rtl8371c_table_read,    /* fMdrv_table_read  */
    rtl8371c_table_write,   /* fMdrv_table_write */
}; /* end of rtl8371c_macdrv */

/* Definition RTL8371C major driver */
rtksw_driver_t rtl8371c_driver =
{
    /* register list   */       rtk_rtl8371c_reg_list,
    /* table list      */       rtk_rtl8371c_table_list,
    /* mac driver      */       &rtl8371c_macdrv,
    /* reg index max   */       RTL8371C_REG_LIST_END,
    /* regField index max */    RTL8371C_REGFIELD_LIST_END,
    /* table index max */       RTL8371C_TABLE_LIST_END,
    /* MAC per port info */     &rtl8371c_macPpInfo,
    /* PTP per port info */     &rtl8371c_macPTPPpInfo
}; /* end of rtl8371c_driver */
