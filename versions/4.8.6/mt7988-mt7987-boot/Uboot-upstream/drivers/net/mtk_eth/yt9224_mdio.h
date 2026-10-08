#ifndef _YT9224_MDIO_H_
#define _YT9224_MDIO_H_

#include "mtk_eth.h"

/* YT9224 默认的 SMI (MDIO) 管理地址（与 yt92xx_uboot.h / DTS phy reg 一致时可覆盖） */
#define YT92XX_SWITCH_PHY_ADDR  0x1d

/* 与 Linux yt92xx / yt9224_uboot 一致的 unit / switch 索引 */
#define YT92XX_SWITCH_ID    0x0

/* YT9224 专用的私有数据结构 */
struct yt9224_switch_priv {
    struct mtk_eth_switch_priv epriv; /* 必须作为第一个成员，以便进行指针强转 */

    struct mii_dev *mdio_bus;
    u8 smi_addr; /* Clause-22 管理 PHY 地址（来自 phy-handle reg） */
    u8 switch_id; /* SMI 访问时的 switch_id（通常为 0） */
    u8 init_done;
};

#endif /* _YT9224_MDIO_H_ */
