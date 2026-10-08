// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 *
 * Author: Neal Yen <neal.yen@mediatek.com>
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#include <phy.h>
#include <miiphy.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/mdio.h>
#include <linux/mii.h>
#include "../mtk_eth.h"

#include "rtk_switch.h"
#include "port.h"
#include "vlan.h"
#include "cpu.h"
#include "dal/smi.h"

#define RTL8366UB_NUM_PORTS          4

struct rtl8366ub_switch_priv {
    struct mtk_eth_switch_priv epriv;
    struct mii_dev *mdio_bus;
    u32 phy_base;
};

static void rtl8366ub_sgmii_config(rtksw_port_t port)
{
    rtksw_port_mac_ability_t mac_cfg;
    rtksw_mode_ext_t mode_ext;

    mode_ext = RTKSW_MODE_EXT_HSGMII;
    mac_cfg.forcemode = PORT_MAC_FORCE;
    mac_cfg.speed = RTKSW_PORT_SPEED_2500M;
    mac_cfg.duplex = RTKSW_PORT_FULL_DUPLEX;
    mac_cfg.link = RTKSW_PORT_LINKUP;
    mac_cfg.nway = RTKSW_DISABLED;
    mac_cfg.txpause = RTKSW_ENABLED;
    mac_cfg.rxpause = RTKSW_ENABLED;
    mac_cfg.speed = RTKSW_PORT_SPEED_2500M;

    rtk_port_macForceLinkExt_set(port, mode_ext, &mac_cfg);
    rtk_port_sgmiiNway_set(port, RTKSW_DISABLED);
}

static int rtl8366ub_mdio_read(struct mii_dev *bus, int addr, int devad, int reg)
{
    struct rtl8366ub_switch_priv *priv = bus->priv;

    if (devad < 0)
        return mtk_mii_read(priv->epriv.eth, addr, reg);

    return mtk_mmd_ind_read(priv->epriv.eth, addr, devad, reg);
}

static int rtl8366ub_mdio_write(struct mii_dev *bus, int addr, int devad, int reg,
                 u16 val)
{

    struct rtl8366ub_switch_priv *priv = bus->priv;

    if (devad < 0)
        return mtk_mii_write(priv->epriv.eth, addr, reg, val);

    return mtk_mmd_ind_write(priv->epriv.eth, addr, devad, reg, val);
}

static int rlt8366ub_mdcreg_read(struct mtk_eth_priv *priv, u8 phy_base, u32 mAddrs, u32 *rData)
{
    if(mAddrs > 0xFFFF)
        return -1;

    if(rData == NULL)
        return -1;

    *rData = 0;

    /* Write address control code to register 31 */
    mtk_mii_write(priv, phy_base, MDC_MDIO_CTRL0_REG, MDC_MDIO_ADDR_OP);

    /* Write address to register 23 */
    mtk_mii_write(priv, phy_base, MDC_MDIO_ADDRESS_REG, mAddrs);

    /* Write read control code to register 21 */
    mtk_mii_write(priv, phy_base, MDC_MDIO_CTRL1_REG, MDC_MDIO_READ_OP);

    /* Read data from register 25 */
    *rData = mtk_mii_read(priv, phy_base, MDC_MDIO_DATA_READ_REG);

    return 0;
}

static void rtl8366ub_mac_control(struct mtk_eth_switch_priv *swpriv, bool enable)
{
    return;
}

static int rtl8366ub_linkstatus_get(struct mtk_eth_switch_priv *swpriv)
{
    rtksw_port_t port;
    rtksw_port_linkStatus_t link;
    rtksw_port_speed_t speed;
    rtksw_port_duplex_t duplex;
    int times = 0;

    printf("%s Waiting for PHY auto negotiation to complete",
           swpriv->sw->name);
    while (times < (CONFIG_PHY_ANEG_TIMEOUT / 50)) {
        for (port = 0; port < RTL8366UB_NUM_PORTS; port++) {
            rtk_port_phyStatus_get(port, &link, &speed, &duplex);
            if (link) {
                printf(" done\n");
                return 0;
            }
        }

        if (ctrlc()) {
            puts("user interrupt!\n");
            return -EINTR;
        }

        if ((times++ % 10) == 0)
            printf(".");

        mdelay(50);    /* 50 ms */
    }

    /*
     * Timeout reached ?
     */
    printf(" TIMEOUT !\n");
    return -ETIMEDOUT;
}

extern void rtk_set_mdc_mdio(struct mtk_eth_priv *priv, u8 id);

static int rtl8366ub_setup(struct mtk_eth_switch_priv *swpriv)
{
    struct rtl8366ub_switch_priv *priv = (struct rtl8366ub_switch_priv *)swpriv;
    int ret, i;
    rtksw_portmask_t portmask = {0};
    rtksw_portmask_t cpu_portmask = {0};
    struct mii_dev *mdio_bus = mdio_alloc();

    if (!mdio_bus)
        return -ENOMEM;

    priv->phy_base = 0;
    mdio_bus->read = rtl8366ub_mdio_read;
    mdio_bus->write = rtl8366ub_mdio_write;
    snprintf(mdio_bus->name, sizeof(mdio_bus->name), priv->epriv.sw->name);

    mdio_bus->priv = priv;

    ret = mdio_register(mdio_bus);
    if (ret) {
        mdio_free(mdio_bus);
        return ret;
    }

    priv->mdio_bus = mdio_bus;

    rtk_set_mdc_mdio(priv->epriv.eth, priv->phy_base);

    rtk_switch_init();
    rtk_vlan_init();
    rtl8366ub_sgmii_config(EXT_PORT1);

    RTKSW_PORTMASK_CLEAR(portmask);
    for (i = 0; i < RTL8366UB_NUM_PORTS; i++) {
        RTKSW_PORTMASK_CLEAR(cpu_portmask);
        RTKSW_PORTMASK_PORT_SET(cpu_portmask, EXT_PORT1);
        rtk_port_isolation_set(i, &cpu_portmask);

        RTKSW_PORTMASK_PORT_SET(portmask, i);
    }
    rtk_port_isolation_set(EXT_PORT1, &portmask);

    return 0;
}

static int rtl8366ub_cleanup(struct mtk_eth_switch_priv *swpriv)
{
    struct rtl8366ub_switch_priv *priv = (struct rtl8366ub_switch_priv *)swpriv;

    mdio_unregister(priv->mdio_bus);

    return 0;
}

static int rtl8366ub_detect(struct mtk_eth_priv *priv)
{
    int ret;
    u32 val;

//    ret = __rtl8366ub_reg_read(priv, 1, 0x10005000, &val);
    ret = rlt8366ub_mdcreg_read(priv, 0, 0x4, &val);
    if (ret) {
        printf("can't get chip ID (%d)\n", ret);
        return ret;
    }

    if (val == 0x8366) {
        printf("found an RTL8366UB switch\n");
        return 0;
    } 

    printf("found an Unknown Realtek switch (id=0x%04x)\n", val);

    return -ENODEV;
}

MTK_ETH_SWITCH(rtl8366ub) = {
    .name = "rtl8366ub",
    .desc = "Realtek RTL8366UB",
    .priv_size = sizeof(struct rtl8366ub_switch_priv),
    .reset_wait_time = 1000,

    .detect = rtl8366ub_detect,
    .setup = rtl8366ub_setup,
    .cleanup = rtl8366ub_cleanup,
    .mac_control = rtl8366ub_mac_control,
    .linkstatus_get = rtl8366ub_linkstatus_get,
};
