
EXTRA_CFLAGS += \
  -DCONFIG_DAL_RTL8371C \
  -DMDC_MDIO_OPERATION \
  -DCONFIG_COMPATIBLE_V1_4 \
  -DDRIVER_VERSION="\\\"$(PKG_VERSION)\\\"" \
  -I$(PKG_BUILD_DIR) \
  -I$(PKG_BUILD_DIR)/dal \
  -I$(PKG_BUILD_DIR)/dal/rtl8371c 

CFLAGS += $(EXTRA_CFLAGS)

rtl8366ub += chip.o
rtl8366ub += port.o
rtl8366ub += vlan.o
rtl8366ub += cpu.o
rtl8366ub += stat.o
rtl8366ub += rtk_switch.o
rtl8366ub += osal/lib.o
rtl8366ub += dal/smi.o
rtl8366ub += dal/dal_mgmts.o
rtl8366ub += dal/reg.o
rtl8366ub += dal/rtl8371c/dal_rtl8371c_cpu.o
rtl8366ub += dal/rtl8371c/dal_rtl8371c_mapper.o
rtl8366ub += dal/rtl8371c/dal_rtl8371c_port.o
rtl8366ub += dal/rtl8371c/dal_rtl8371c_vlan.o
rtl8366ub += dal/rtl8371c/dal_rtl8371c_switch.o
rtl8366ub += dal/rtl8371c/rtk_rtl8371c_reg_list.o
rtl8366ub += dal/rtl8371c/rtk_rtl8371c_regField_list.o
rtl8366ub += dal/rtl8371c/rtk_rtl8371c_macdrv.o
rtl8366ub += dal/rtl8371c/rtk_rtl8371c_table_list.o
rtl8366ub += dal/rtl8371c/rtk_rtl8371c_tableField_list.o
rtl8366ub += dal/rtl8371c/rtl8371c_chip.o
rtl8366ub += dal/rtl8371c/dal_rtl8371c_stat.o

obj-y += rtl8366ub_mdio.o $(rtl8366ub)
