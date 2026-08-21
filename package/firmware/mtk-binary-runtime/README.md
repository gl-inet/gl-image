# MTK 二进制运行时边界

本目录只封装无法随客户 SDK 公开源码的 MTK 运行时 IPK，不包含私有构建系统或私有源码树。构建时只解包 IPK 的运行时数据段，不执行其中的私有构建逻辑。

- `mtk-wifi-runtime`：MT7993 Wi-Fi KO、Wi-Fi 固件、MTK 定制的 hostapd/wpad、控制程序、KVC 运行库与配置脚本。
- `mtk-phy-firmware`：MT7987 内置 2.5G PHY 加载的固件。PHY 驱动本身从公开源码构建。
- `manifests/`：原始 IPK 与解包后文件的 SHA-256 清单。

Wi-Fi KO 仅允许与 Linux `5.4.281`、MT7987 目标以及经过 ABI 检查的内核组合使用。升级内核、切换内核配置或修改相关导出符号后，必须重新取得匹配的 KO 并执行 ABI 检查。
