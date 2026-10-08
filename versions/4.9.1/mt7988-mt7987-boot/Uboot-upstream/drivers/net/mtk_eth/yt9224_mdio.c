#include <common.h>
#include <dm.h>
#include <env.h>
#include <miiphy.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/ofnode.h>
#include <dm/read.h>
#include <asm/gpio.h>
#include "mtk_eth.h"
#include "yt9224_mdio.h"

static bool yt9224_is_ba7200be(void)
{
    return of_machine_is_compatible("gl,ba7200be");
}

static int yt9224_switch_write(struct mtk_eth_priv *priv, u8 devaddr, u8 switch_id,
                               u32 reg, u32 val);


__weak void yt9224_board_isolation(struct mtk_eth_priv *priv, u8 devaddr,
                                   u8 switch_id)
{
    /*
     * PORT_ISO_CTRLN: base = 0x180d80, stride = 4, N = 0~9
     * ISO_PORT_MASK (bits 9:0): bit i = 1 means DROP packets to port i
     *   (1 = block/filtered, 0 = allow/forward)
     *
     * YT9224 port layout:
     *   Port 0:   SerDes  (CPU-facing, to SoC GMAC)
     *   Port 4-7: PHY     (user-facing RJ45)
     *   Port 8:   SerDes  (CPU-facing, to SoC GMAC)
     *   Port 9:   CPU Port (management)
     *
     * Goal: user ports (4-7) can only communicate with CPU-facing
     *       ports (0, 8, 9) and cannot communicate with each other.
     */
    static const u32 iso_reg_base = 0x00180d80;
    static const int user_ports[] = { 4, 5, 6, 7 };
    static const u32 user_port_mask = BIT(4) | BIT(5) | BIT(6) | BIT(7);
    u32 iso_mask;
    int i;
    int ret;

    if (!priv)
        return;

    for (i = 0; i < ARRAY_SIZE(user_ports); i++) {
        /*
         * Block traffic from this user port to all OTHER user ports.
         * Allow traffic to CPU-facing ports (0, 8, 9) and self.
         *   iso_mask = user_port_mask & ~BIT(user_ports[i])
         */
        iso_mask = user_port_mask & ~BIT(user_ports[i]);

        ret = yt9224_switch_write(priv, devaddr, switch_id,
                                  iso_reg_base + (user_ports[i] * 4),
                                  iso_mask);
        if (ret < 0) {
            printf("YT9224: port isolation write failed for port %d (ret=%d)\n",
                   user_ports[i], ret);
            return;
        }
    }

    printf("YT9224: port isolation configured (PHY ports 4-7 isolated from each other)\n");
}

/*
 * 辅助函数：由于无法直接访问不完整类型 struct mtk_eth_priv 的成员，
 * 通过遍历 UCLASS_ETH 设备来找到当前 eth 对应的 udevice，
 * 进而解析 phy-handle 节点中的 reset-gpios 和 reg 属性。
 */
static struct udevice *yt9224_find_eth_dev(struct mtk_eth_priv *eth)
{
    struct uclass *uc;
    struct udevice *dev;

    if (!uclass_get(UCLASS_ETH, &uc)) {
        uclass_foreach_dev(dev, uc) {
            if (dev_get_priv(dev) == eth)
                return dev;
        }
    }
    return NULL;
}

static ofnode yt9224_get_phy_node(struct mtk_eth_priv *eth)
{
    struct udevice *dev = yt9224_find_eth_dev(eth);
    struct ofnode_phandle_args args;

    if (dev && !dev_read_phandle_with_args(dev, "phy-handle", NULL, 0, 0, &args))
        return args.node;

    return ofnode_null();
}

static int yt9224_get_phy_addr(struct mtk_eth_priv *eth)
{
    ofnode node = yt9224_get_phy_node(eth);
    if (ofnode_valid(node))
        return ofnode_read_s32_default(node, "reg", -1);
    return -1;
}

static void yt9224_do_reset(struct mtk_eth_priv *eth)
{
    ofnode node = yt9224_get_phy_node(eth);
    struct gpio_desc reset_gpio;
    u32 assert_us, deassert_us;

    if (ofnode_valid(node) &&
            !gpio_request_by_name_nodev(node, "reset-gpios", 0, &reset_gpio,
                                        GPIOD_IS_OUT | GPIOD_ACTIVE_LOW)) {
        assert_us = ofnode_read_u32_default(node, "reset-assert-us", 20000);
        deassert_us = ofnode_read_u32_default(node, "reset-deassert-us", 200000);

        dm_gpio_set_value(&reset_gpio, 1);
        if (assert_us)
            udelay(assert_us);
        dm_gpio_set_value(&reset_gpio, 0);
        if (deassert_us)
            udelay(deassert_us);

        dm_gpio_free(NULL, &reset_gpio);
    } else {
        /* 旧 DTS 无 GPIO：仅保留 settle 延时 */
        udelay(50000);
        udelay(50000);
    }
}

/*
 * YT9224 寄存器访问宏与定义
 */
#define REG_ADDR_BIT1_ADDR    0
#define REG_ADDR_BIT1_DATA    1
#define REG_ADDR_BIT0_WRITE    0
#define REG_ADDR_BIT0_READ    1
#define MAX_BUSYING_WAIT_TIME    200
#define YT9224_MDIO_YIELD_US    200

#define YT9224_MDIO_CTRL_BASE(n)    (0x000f1000 - ((n) * 0x1000))
#define YT9224_MDIO_OPT_CTRL_REG(n)    (YT9224_MDIO_CTRL_BASE(n))
#define YT9224_MDIO_ADDR_CTRL_REG(n)    (YT9224_MDIO_CTRL_BASE(n) + 0x4)
#define YT9224_MDIO_DATA_0_REG(n)    (YT9224_MDIO_CTRL_BASE(n) + 0x8)
#define YT9224_MDIO_DATA_1_REG(n)    (YT9224_MDIO_CTRL_BASE(n) + 0xc)

#define YT9224_INTPHY_MDIO_ID        1

static void __maybe_unused yt9224_mdio_yield(void)
{
    udelay(YT9224_MDIO_YIELD_US);
}

static u16 __maybe_unused yt9224_mdio_data1_to_u16(u32 w)
{
    u16 lo = (u16)(w & 0xffff);
    u16 hi = (u16)((w >> 16) & 0xffff);

    if (hi == lo)
        return lo;
    if (lo && lo != 0xffff)
        return lo;
    if (hi && hi != 0xffff)
        return hi;
    return lo;
}

/*
 * 通过 MDIO 写入 YT9224 寄存器
 */
static int __maybe_unused yt9224_switch_write(struct mtk_eth_priv *priv, u8 devaddr, u8 switch_id,
        u32 reg, u32 val)
{
    u8 regAddr;
    u16 regVal;
    int ret;

    regAddr = (switch_id << 2) | (REG_ADDR_BIT1_ADDR << 1) |
              REG_ADDR_BIT0_WRITE;
    regVal = (reg >> 16) & 0xffff;
    ret = mtk_mii_write(priv, devaddr, regAddr, regVal);
    if (ret < 0)
        return ret;

    regVal = reg & 0xffff;
    ret = mtk_mii_write(priv, devaddr, regAddr, regVal);
    if (ret < 0)
        return ret;

    regAddr = (switch_id << 2) | (REG_ADDR_BIT1_DATA << 1) |
              REG_ADDR_BIT0_WRITE;
    regVal = (val >> 16) & 0xffff;
    ret = mtk_mii_write(priv, devaddr, regAddr, regVal);
    if (ret < 0)
        return ret;

    regVal = val & 0xffff;
    return mtk_mii_write(priv, devaddr, regAddr, regVal);
}

/*
 * 通过 MDIO 读取 YT9224 寄存器
 */
static int __maybe_unused yt9224_switch_read(struct mtk_eth_priv *priv, u8 devaddr, u8 switch_id,
        u32 reg, u32 *val)
{
    u8 regAddr;
    u16 regVal;
    int ret;
    u32 rdata;

    if (!val)
        return -EINVAL;

    regAddr = (switch_id << 2) | (REG_ADDR_BIT1_ADDR << 1) |
              REG_ADDR_BIT0_READ;
    regVal = (reg >> 16) & 0xffff;
    ret = mtk_mii_write(priv, devaddr, regAddr, regVal);
    if (ret < 0)
        return ret;

    regVal = reg & 0xffff;
    ret = mtk_mii_write(priv, devaddr, regAddr, regVal);
    if (ret < 0)
        return ret;

    regAddr = (switch_id << 2) | (REG_ADDR_BIT1_DATA << 1) |
              REG_ADDR_BIT0_READ;
    ret = mtk_mii_read(priv, devaddr, regAddr);
    if (ret < 0)
        return ret;
    rdata = (u32)ret << 16;

    ret = mtk_mii_read(priv, devaddr, regAddr);
    if (ret < 0)
        return ret;
    rdata |= (u16)ret;

    *val = rdata;
    return 0;
}

/* 内部 MDIO 读取 */
static int __maybe_unused yt9224_internal_mdio_read(struct mtk_eth_priv *priv, u8 devaddr,
        u8 switch_id, int phy_addr, int regnum,
        int mdio_id, u16 *val)
{
    u32 base_data = 0, op_data = 0;
    u32 wait_count = 0;
    int ret;

    if (!val)
        return -EINVAL;

    ret = yt9224_switch_read(priv, devaddr, switch_id,
                             YT9224_MDIO_ADDR_CTRL_REG(mdio_id),
                             &base_data);
    if (ret < 0)
        return ret;

    base_data &= 0xFC00FFF1;
    base_data |= ((phy_addr & 0x1f) << 21) | ((regnum & 0x1f) << 16) |
                 (2 << 2);

    ret = yt9224_switch_write(priv, devaddr, switch_id,
                              YT9224_MDIO_ADDR_CTRL_REG(mdio_id),
                              base_data);
    if (ret < 0)
        return ret;

    ret = yt9224_switch_write(priv, devaddr, switch_id,
                              YT9224_MDIO_OPT_CTRL_REG(mdio_id), 1);
    if (ret < 0)
        return ret;

    while (wait_count++ < MAX_BUSYING_WAIT_TIME) {
        ret = yt9224_switch_read(priv, devaddr, switch_id,
                                 YT9224_MDIO_OPT_CTRL_REG(mdio_id),
                                 &op_data);
        if (ret < 0)
            return ret;
        if (!op_data) {
            ret = yt9224_switch_read(priv, devaddr, switch_id,
                                     YT9224_MDIO_DATA_1_REG(mdio_id),
                                     &base_data);
            if (ret < 0)
                return ret;
            *val = yt9224_mdio_data1_to_u16(base_data);
            yt9224_mdio_yield();
            return 0;
        }
        udelay(10);
    }

    return -ETIMEDOUT;
}

/* 内部 MDIO 写入 */
static int __maybe_unused yt9224_internal_mdio_write(struct mtk_eth_priv *priv, u8 devaddr,
        u8 switch_id, int phy_addr, int regnum,
        int mdio_id, u16 val)
{
    u32 base_data = 0, op_data = 0;
    u32 wait_count = 0;
    int ret;

    ret = yt9224_switch_read(priv, devaddr, switch_id,
                             YT9224_MDIO_ADDR_CTRL_REG(mdio_id),
                             &base_data);
    if (ret < 0)
        return ret;

    base_data &= 0xFC00FFF1;
    base_data |= ((phy_addr & 0x1f) << 21) | ((regnum & 0x1f) << 16) |
                 (1 << 2);

    ret = yt9224_switch_write(priv, devaddr, switch_id,
                              YT9224_MDIO_ADDR_CTRL_REG(mdio_id),
                              base_data);
    if (ret < 0)
        return ret;

    ret = yt9224_switch_write(priv, devaddr, switch_id,
                              YT9224_MDIO_DATA_0_REG(mdio_id), val);
    if (ret < 0)
        return ret;

    ret = yt9224_switch_write(priv, devaddr, switch_id,
                              YT9224_MDIO_OPT_CTRL_REG(mdio_id), 1);
    if (ret < 0)
        return ret;

    while (wait_count++ < MAX_BUSYING_WAIT_TIME) {
        ret = yt9224_switch_read(priv, devaddr, switch_id,
                                 YT9224_MDIO_OPT_CTRL_REG(mdio_id),
                                 &op_data);
        if (ret < 0)
            return ret;
        if (!op_data) {
            yt9224_mdio_yield();
            return 0;
        }
        udelay(10);
    }

    return -ETIMEDOUT;
}

/* 内部 PHY 扩展寄存器写入 */
static int __maybe_unused yt9224_internal_phy_ext_write(struct mtk_eth_priv *priv, u8 devaddr,
        u8 switch_id, int phy_addr, int mdio_id,
        u16 reg_addr, u16 val)
{
    int ret;

    ret = yt9224_internal_mdio_write(priv, devaddr, switch_id, phy_addr,
                                     0x1e, mdio_id, reg_addr);
    if (ret < 0)
        return ret;

    return yt9224_internal_mdio_write(priv, devaddr, switch_id, phy_addr,
                                      0x1f, mdio_id, val);
}

/*
 * 内部 PHY 扩展寄存器读取
 */
static int __maybe_unused yt9224_internal_phy_ext_read(struct mtk_eth_priv *priv, u8 devaddr,
        u8 switch_id, int phy_addr, int mdio_id,
        u16 reg_addr, u16 *val)
{
    int ret;

    ret = yt9224_internal_mdio_write(priv, devaddr, switch_id, phy_addr,
                                     0x1e, mdio_id, reg_addr);
    if (ret < 0)
        return ret;

    return yt9224_internal_mdio_read(priv, devaddr, switch_id, phy_addr,
                                     0x1f, mdio_id, val);
}

static void yt9224_serdes_usxgmii_10g_phy_ext_init(struct mtk_eth_priv *priv,
        u8 devaddr, u8 switch_id,
        int phy_clause_addr)
{
    const int mdio = YT9224_INTPHY_MDIO_ID;
    u16 v;
    static const struct {
        u16 reg;
        u16 val;
    } sds_seq[] = {
        { 0x0406, 0x0800 }, { 0x0416, 0x4558 }, /* CDR */
        { 0x043a, 0x1006 }, { 0x043f, 0x3029 }, { 0x042a, 0xf070 }, /* PLL */
        { 0x0439, 0x00c0 }, /* VCO */
        { 0x0492, 0x7f7f }, { 0x0491, 0x007f }, /* VDAC */
        { 0x0454, 0x0f14 }, { 0x0497, 0x0a44 }, { 0x04cd, 0x0000 }, /* eye */
        { 0x04af, 0x45e3 }, { 0x048a, 0x0fff }, { 0x0408, 0x7c00 },
        { 0x04d6, 0x007f }, { 0x044f, 0xff08 },
        { 0x048e, 0x7d00 }, { 0x000d, 0x060f }, /* FFE */
        { 0x04b0, 0x0804 }, { 0x04b1, 0x7774 }, { 0x04af, 0x45e7 }, /* CTLE */
        { 0x0003, 0x5603 },
    };
    int i;

    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x04be, 0x000d);
    for (i = 0; i < ARRAY_SIZE(sds_seq); i++)
        (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                            mdio, sds_seq[i].reg, sds_seq[i].val);

    /* Toggle VDAC and pulse restart controls to make RX CDR lock robust. */
    udelay(20000);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x0492, 0x7fff);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x0492, 0x7f7f);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x2000, 0x0040);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x2000, 0x0000);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x1000, 0x1721);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x1000, 0x1720);
    /*
     * Align to Linux steady state on BE14000:
     * intphy read <addr> 0 -> 0x0100 (FDX only, AN/restart bits cleared).
     * Keeping 0xa100 here leaves BMCR in transient bring-up mode in U-Boot.
     */
    (void)yt9224_internal_mdio_write(priv, devaddr, switch_id, phy_clause_addr,
                                     0x0, mdio, 0x0100);

    if (yt9224_internal_phy_ext_read(priv, devaddr, switch_id, phy_clause_addr,
                                     mdio, 0x400, &v) < 0)
        return;
    v &= (u16)~(0x7u << 4);
    v |= (u16)(0x6 << 4);
    (void)yt9224_internal_phy_ext_write(priv, devaddr, switch_id, phy_clause_addr,
                                        mdio, 0x400, v);
}

/*
 * 应用 MDI Swap (移植自 yt92xx_kernel_port.c)
 * 对应 Linux fal_whale_port.c 中对内部 PHY 的 0xa000 寄存器写入 0x780
 */
static int yt9224_apply_mdi_swap(struct mtk_eth_priv *priv, u8 devaddr, u8 switch_id)
{
    u16 val;
    int ret;

    ret = yt9224_internal_phy_ext_write(priv, devaddr, switch_id, 9,
                                        YT9224_INTPHY_MDIO_ID, 0xa000, 0x0780);

    yt9224_internal_phy_ext_read(priv, devaddr, switch_id, 9, YT9224_INTPHY_MDIO_ID, 0xa000, &val);
    if (val != 0x0780) {
        return -EIO;
    }

    return ret;

}

/*
 * 强制指定 MAC 端口为 10G USXGMII 模式并关闭 AN (移植自 gl_be14000_yt9224_min.c)
 */
#define YT9224_REG_PORT_CTRL(mac)    (0x00080080 + ((mac) * 4))
#define YT9224_PORT_CTRL_FORCE_OP    (1u << 14)
#define YT9224_PORT_CTRL_AN_LINK_EN    (1u << 10)
#define YT9224_PORT_CTRL_LINK        (1u << 9)
#define YT9224_PORT_CTRL_DUPLEX_FULL    (1u << 7)
#define YT9224_PORT_CTRL_RXMAC_EN    (1u << 4)
#define YT9224_PORT_CTRL_TXMAC_EN    (1u << 3)
#define YT9224_PORT_SPEED_10G        3
#define YT9224_PORT_CTRL_CFG_RXMAC_EN    (1u << 12)
#define YT9224_PORT_CTRL_CFG_TXMAC_EN    (1u << 13)

#define YT9224_PORT_CTRL_MASK_FORCE_10G_FULL_UP \
    (YT9224_PORT_CTRL_FORCE_OP | YT9224_PORT_CTRL_LINK | \
     YT9224_PORT_CTRL_RXMAC_EN | YT9224_PORT_CTRL_TXMAC_EN | \
     YT9224_PORT_CTRL_DUPLEX_FULL | YT9224_PORT_SPEED_10G)

#define YT9224_PORT_CTRL_MASK_FORCE_10G_FULL_UP_CFG \
    (YT9224_PORT_CTRL_MASK_FORCE_10G_FULL_UP | \
     YT9224_PORT_CTRL_CFG_RXMAC_EN | YT9224_PORT_CTRL_CFG_TXMAC_EN)

/*
 * PORT_STATUSm base = 0x80200, stride = 4, LINK bit = 8
 */
#define YT9224_REG_PORT_STATUS(mac)    (0x00080200 + ((mac) * 4))
#define YT9224_PORT_STATUS_LINK        BIT(8)

#ifndef CONFIG_PHY_ANEG_TIMEOUT
#define CONFIG_PHY_ANEG_TIMEOUT    40
#endif

static void yt9224_port_force_usxgmii_10g(struct mtk_eth_priv *priv, u8 devaddr,
        u8 switch_id, int mac)
{
    u32 ctrl = 0;
    u32 reg = YT9224_REG_PORT_CTRL(mac);

    if (yt9224_switch_read(priv, devaddr, switch_id, reg, &ctrl) < 0)
        return;
    ctrl &= ~0x3fffU;
    ctrl |= YT9224_PORT_CTRL_MASK_FORCE_10G_FULL_UP_CFG;
    ctrl &= ~YT9224_PORT_CTRL_AN_LINK_EN;
    yt9224_switch_write(priv, devaddr, switch_id, reg,
                        ctrl | YT9224_PORT_CTRL_FORCE_OP);
}

/* Enable RJ45 UTP ports (4-7) autoneg for BA7200BE LAN block */
static void yt9224_utp_ports_enable(struct mtk_eth_priv *priv, u8 devaddr,
                                    u8 switch_id)
{
    static const int utp_ports[] = { 4, 5, 6, 7 };
    u32 reg, ctrl;
    int i;

    for (i = 0; i < ARRAY_SIZE(utp_ports); i++) {
        reg = YT9224_REG_PORT_CTRL(utp_ports[i]);
        if (yt9224_switch_read(priv, devaddr, switch_id, reg, &ctrl) < 0)
            continue;
        ctrl &= ~(YT9224_PORT_CTRL_FORCE_OP |
                  YT9224_PORT_CTRL_LINK |
                  YT9224_PORT_CTRL_MASK_FORCE_10G_FULL_UP_CFG);
        ctrl |= YT9224_PORT_CTRL_AN_LINK_EN |
                YT9224_PORT_CTRL_RXMAC_EN |
                YT9224_PORT_CTRL_TXMAC_EN |
                YT9224_PORT_CTRL_CFG_RXMAC_EN |
                YT9224_PORT_CTRL_CFG_TXMAC_EN;
        yt9224_switch_write(priv, devaddr, switch_id, reg, ctrl);
    }
}

/* 1. 探测函数：用于自动识别芯片 */
static int yt9224_detect(struct mtk_eth_priv *eth)
{
    int id1, id2;
    int phy_addr;
    u8 smi_addr;

    if (!eth)
        return -ENODEV;

    phy_addr = yt9224_get_phy_addr(eth);
    smi_addr = (phy_addr >= 0) ? (u8)phy_addr : YT92XX_SWITCH_PHY_ADDR;

    /* 尝试通过标准 Clause-22 读取 PHY ID 寄存器 */
    id1 = mtk_mii_read(eth, smi_addr, MII_PHYSID1);
    id2 = mtk_mii_read(eth, smi_addr, MII_PHYSID2);

    if (id1 < 0 || id2 < 0)
        return -ENODEV;

    /*
     * YT9224 的 Switch CPU Port 在标准 Clause-22 下读取 PHY ID
     * 预期会返回 0xdead:0xdead
     */
    if ((id1 & 0xffff) == 0xdead && (id2 & 0xffff) == 0xdead) {
        printf("YT9224: Detected switch at SMI address 0x%02x\n", smi_addr);
        return 0;
    }

    /* 默认返回 0，允许通过设备树强制绑定 (mediatek,switch = "yt9224") */
    return 0;
}

/*
 * 2. 初始化与配置（对齐 yt9224_uboot.c::yt9224_uboot_after_phy_probe 的核心步骤）
 *
 * - mtk_switch_init() 已做过一次短时 GPIO/复位；此处按 Motorcomm 文档再发一轮
 *   长脉宽复位（mtk_eth_switch_yt9224_reset_pulse），无 GPIO 时退化为延时。
 * - Clause-22 读 PHYID（常见为 0xdead:0xdead），再调 board_isolation 弱钩子。
 * 完整 SDK 级 VLAN/端口表等仍在板级 yt9224_board_isolation 或后续扩展中完成。
 */
static int yt9224_setup(struct mtk_eth_switch_priv *swpriv)
{
    struct yt9224_switch_priv *priv = (struct yt9224_switch_priv *)swpriv;
    struct mtk_eth_priv *eth = swpriv->eth;
    int phy_addr;
    u8 devaddr;
    u8 switch_id = YT92XX_SWITCH_ID;
    int id1, id2;
    int ret;

    if (!eth)
        return -EINVAL;

    phy_addr = yt9224_get_phy_addr(eth);
    devaddr = (phy_addr >= 0) ? (u8)phy_addr : YT92XX_SWITCH_PHY_ADDR;
    priv->smi_addr = devaddr;
    priv->switch_id = switch_id;

    /*
     * 框架 mtk_switch_init() 仅处理了 eth 节点的 rst_gpio，
     * 此处解析 phy-handle 节点中的 reset-gpios 并执行复位。
     */
    yt9224_do_reset(eth);

    id1 = mtk_mii_read(eth, devaddr, MII_PHYSID1);
    id2 = mtk_mii_read(eth, devaddr, MII_PHYSID2);
    if (id1 < 0 || id2 < 0) {
        printf("YT9224: clause-22 PHY ID read failed\n");
        return -EIO;
    }

    if (((id1 & 0xffff) == 0xdead) && ((id2 & 0xffff) == 0xdead))
        printf("YT9224: PHYID %04x:%04x (switch CPU port; extended MDIO path)\n",
               id1 & 0xffff, id2 & 0xffff);
    else
        printf("YT9224: PHYID %04x:%04x (extended MDIO switch path)\n",
               id1 & 0xffff, id2 & 0xffff);

    /* 应用 MDI Swap */
    ret = yt9224_apply_mdi_swap(eth, devaddr, switch_id);
    if (ret < 0)
        printf("YT9224: failed to apply MDI swap (ret = %d)\n", ret);

    /* 强制 MAC8 (CPU) 和 MAC0 为 10G USXGMII */
    yt9224_port_force_usxgmii_10g(eth, devaddr, switch_id, 8);
    yt9224_port_force_usxgmii_10g(eth, devaddr, switch_id, 0);

    /* SerDes / USXGMII init for MAC0 and MAC8 */
    yt9224_serdes_usxgmii_10g_phy_ext_init(eth, devaddr, switch_id, 0);
    yt9224_serdes_usxgmii_10g_phy_ext_init(eth, devaddr, switch_id, 8);

    printf("YT9224: calling board_isolation (minimal)\n");
    yt9224_board_isolation(eth, devaddr, switch_id);
    if (yt9224_is_ba7200be())
        yt9224_utp_ports_enable(eth, devaddr, switch_id);

    priv->init_done = 1;
    return 0;
}

/* 3. 清理函数 */
static int yt9224_cleanup(struct mtk_eth_switch_priv *swpriv)
{
    struct yt9224_switch_priv *priv = (struct yt9224_switch_priv *)swpriv;

    if (priv->mdio_bus) {
        mdio_unregister(priv->mdio_bus);
        priv->mdio_bus = NULL;
    }
    priv->smi_addr = 0;
    priv->switch_id = 0;
    priv->init_done = 0;
    return 0;
}

/* 4. MAC 控制函数：在网络启动/停止时调用 */
static void yt9224_mac_control(struct mtk_eth_switch_priv *swpriv, bool enable)
{
    struct yt9224_switch_priv *priv = (struct yt9224_switch_priv *)swpriv;
    struct mtk_eth_priv *eth;
    const int cpu_macs[] = { 0, 8 };
    int i;
    u32 reg, ctrl;

    if (!priv || !priv->init_done)
        return;

    eth = swpriv->eth;
    if (!eth)
        return;

    for (i = 0; i < ARRAY_SIZE(cpu_macs); i++) {
        reg = YT9224_REG_PORT_CTRL(cpu_macs[i]);

        if (enable) {
            yt9224_port_force_usxgmii_10g(eth, priv->smi_addr,
                                          priv->switch_id, cpu_macs[i]);
            continue;
        }

        if (yt9224_switch_read(eth, priv->smi_addr, priv->switch_id,
                               reg, &ctrl) < 0)
            continue;

        ctrl &= ~(YT9224_PORT_CTRL_LINK |
                  YT9224_PORT_CTRL_RXMAC_EN | YT9224_PORT_CTRL_TXMAC_EN |
                  YT9224_PORT_CTRL_CFG_RXMAC_EN | YT9224_PORT_CTRL_CFG_TXMAC_EN);
        ctrl |= YT9224_PORT_CTRL_FORCE_OP;

        (void)yt9224_switch_write(eth, priv->smi_addr, priv->switch_id,
                                  reg, ctrl);
    }
}

/* 5. 链路状态获取函数 */
static int yt9224_linkstatus_get(struct mtk_eth_switch_priv *swpriv)
{
    struct yt9224_switch_priv *priv = (struct yt9224_switch_priv *)swpriv;
    struct mtk_eth_priv *eth;
    int port_phy;
    bool is_switch_mgmt_addr;
    int timeout_ms;
    int timeout_cnt;
    int times = 0;
    int mac;
    u16 bmsr;
    u32 st;

    if (!priv || !priv->init_done)
        return -EIO;

    eth = swpriv->eth;
    if (!eth)
        return -EINVAL;

    port_phy = yt9224_get_phy_addr(eth);
    is_switch_mgmt_addr = (port_phy == priv->smi_addr);
    timeout_ms = CONFIG_PHY_ANEG_TIMEOUT;

    /*
     * ethpluglock 场景下尽快失败，避免每次扫描都等满默认自协商超时。
     * 可通过 ethswlinkwait（毫秒）覆盖，默认 600ms。
     */
    if (env_get_yesno("ethpluglock") == 1)
        timeout_ms = env_get_ulong("ethswlinkwait", 10, 600);

    timeout_cnt = timeout_ms / 50;
    if (timeout_cnt <= 0)
        timeout_cnt = 1;
    while (times < timeout_cnt) {
        /*
         * eth1 的 phy-handle 在本项目 DTS 中指向 YT9224 管理地址（如 29），
         * 不是某个前面板端口 PHY 地址。此时应读取 switch PORT_STATUS。
         */
        if (!is_switch_mgmt_addr && port_phy >= 0 && port_phy <= 0x1f) {
            if (!yt9224_internal_mdio_read(eth, priv->smi_addr, priv->switch_id,
                                           port_phy, MII_BMSR, YT9224_INTPHY_MDIO_ID,
                                           &bmsr) &&
                    !yt9224_internal_mdio_read(eth, priv->smi_addr, priv->switch_id,
                                               port_phy, MII_BMSR, YT9224_INTPHY_MDIO_ID,
                                               &bmsr) &&
                    (bmsr & BMSR_LSTATUS)) {
                return 0;
            }
        } else {
            int mac_first = yt9224_is_ba7200be() ? 4 : 1;
            int mac_last = 7;

            for (mac = mac_first; mac <= mac_last; mac++) {
                if (yt9224_switch_read(eth, priv->smi_addr, priv->switch_id,
                                       YT9224_REG_PORT_STATUS(mac), &st) < 0)
                    continue;

                if (st & YT9224_PORT_STATUS_LINK) {
                    return 0;
                }
            }
        }

        if (ctrlc()) {
            puts("user interrupt!\n");
            return -EINTR;
        }
        if ((times++ % 10) == 0)

            mdelay(50);
    }
    return -ENETDOWN;
}

/* 注册 Switch 驱动到 mtk_eth 框架 */
MTK_ETH_SWITCH(yt9224) = {
    .name = "yt9224",
    .desc = "Motorcomm YT9224 Switch",
    .priv_size = sizeof(struct yt9224_switch_priv),
    .reset_wait_time = 200, /* 硬件复位后需要等待的时间 (ms) */

    .detect = yt9224_detect,
    .setup = yt9224_setup,
    .cleanup = yt9224_cleanup,
    .mac_control = yt9224_mac_control,
    .linkstatus_get = yt9224_linkstatus_get,
};
