# GL-BE10000 Bootloader

For normal operating-system firmware upgrades, see
[Install the firmware](../GL-BE10000_FIRMWARE_GUIDE.md#install-the-firmware).
This README covers building and installing the bootloader images.

This directory contains U-Boot 2025.04-rc3 and the MediaTek TF-A snapshot
`atf-20250304-5ea9f9736` for GL-BE10000 (MT7987). The bootloader is built
separately from the OpenWrt firmware in the parent directory.

## Build Environment

Use a case-sensitive GNU/Linux filesystem. On Ubuntu, install:

```sh
sudo apt install build-essential gcc-aarch64-linux-gnu \
  device-tree-compiler bison flex libssl-dev python3 python3-pyelftools
```

Install these dependencies before building without network access. The platform
DRAM and eFuse objects supplied with the TF-A snapshot are retained under
`atf-20250304-5ea9f9736/plat/mediatek/mt7987/drivers/` as build inputs.

## Build U-Boot

Run from this directory:

```sh
export CROSS_COMPILE=aarch64-linux-gnu-
make -C Uboot-upstream gl_be10000_defconfig
make -C Uboot-upstream -j"$(nproc)"
```

The output is `Uboot-upstream/u-boot.bin`.

## Build TF-A

Run from this directory after building U-Boot:

```sh
cp Uboot-upstream/u-boot.bin atf-20250304-5ea9f9736/u-boot.bin
make -C atf-20250304-5ea9f9736 gl_be10000_defconfig
make -C atf-20250304-5ea9f9736 -j"$(nproc)"
```

The boot images are:

```text
atf-20250304-5ea9f9736/build/mt7987/release/fip.bin
atf-20250304-5ea9f9736/build/mt7987/release/bl2.img
```

## Image Purpose and Supported Hardware

| Build output | Contents | Installation target |
| --- | --- | --- |
| `Uboot-upstream/u-boot.bin` | Raw U-Boot payload (BL33). | Input to the TF-A build. |
| `atf-20250304-5ea9f9736/build/mt7987/release/fip.bin` | TF-A FIP containing BL31 and the U-Boot BL33 payload. | Upload to `/uboot.html`, or use the serial `mtkupgrade fip` operation. |
| `atf-20250304-5ea9f9736/build/mt7987/release/bl2.img` | First-stage boot image; initializes DRAM and loads FIP. | The separate serial `mtkupgrade bl2` operation. |

These images are for **GL-BE10000** with the hardware configuration below:

- SoC: MT7987; DDR4; SPI NAND with the TF-A `4K+256` page/OOB format.
- U-Boot configuration: `gl_be10000_defconfig`; device tree:
  `Uboot-upstream/arch/arm/dts/gl-be10000.dts`.
- TF-A configuration: `atf-20250304-5ea9f9736/configs/gl_be10000_defconfig`.
- The device-tree boot partitions are `bl2` (offset `0x0`, size
  `0x100000`), `u-boot-env` (offset `0x100000`, size `0x80000`),
  and `fip` (offset `0x580000`, size `0x300000`). The intervening
  `factory` partition contains device-specific data and must be preserved.
- Use the named-partition upgrade commands below; NAND bad blocks and ECC are
  handled by the storage code. These offsets describe the layout, not commands
  for writing the whole flash.

This source release identifies hardware through the board configurations and
device tree; it does not enumerate PCB revision numbers. Check the model and
PCB revision printed on the device/board and match its storage and DRAM to this
profile before installing. A different flash geometry, boot layout, DRAM type,
or secure-boot configuration requires a matching build. The source does not
establish compatibility with every PCB revision or with secure-boot-fused units.
Do not use GL-BE10000 images on GL-BE14000 or vice versa.

Boot images are not sysupgrade images. OpenWrt `sysupgrade` and the normal
GL.iNet firmware upgrade page install the operating-system firmware; they do
not install these boot images. The U-Boot recovery server is a separate program
running before Linux. Its **U-BOOT UPDATE** page is the installation interface
described below.

## Install FIP Using the U-Boot Web Interface

This method updates FIP, including U-Boot and BL31. It preserves the installed
BL2. Use the serial procedure below when BL2 must also be replaced.

### Prepare the Image and PC

1. Build U-Boot and TF-A using the instructions above. Use the `fip.bin`
   produced for this model and this hardware profile.
2. Record the checksums on the build PC:

   ```sh
   sha256sum atf-20250304-5ea9f9736/build/mt7987/release/fip.bin
   md5sum atf-20250304-5ea9f9736/build/mt7987/release/fip.bin
   ```

   SHA256 identifies the image in the installation record. MD5 is used only
   to compare with the uploaded-file checksum displayed by the Web page.
3. Prepare a PC with an Ethernet port, Ethernet cable, and Chrome or Edge.
   Connect it directly to a router LAN port. Configure the PC Ethernet adapter
   with static IPv4 address `192.168.1.2` and subnet mask
   `255.255.255.0`; no gateway or DNS is needed on this direct link.
   Disable other connections that route `192.168.1.0/24`.
4. Keep a known-good boot image for this exact device and stable power available.
   A failed bootloader write can prevent both normal boot and Web recovery.

### Enter Recovery and Install

1. Power off the router. Hold the **Reset** button, apply power, and continue
   holding Reset during startup until the router enters U-Boot recovery mode;
   then release it. Follow the device's recovery indication. A Reset press
   after Linux starts invokes a different operation.
2. Open `http://192.168.1.1/uboot.html`. Confirm that the page says
   **U-BOOT UPDATE**, rather than the ordinary firmware update page.
3. Select the model's TF-A **`fip.bin`**. Do not select the raw
   `Uboot-upstream/u-boot.bin`, `bl2.img`, a sysupgrade image, or a
   whole-flash image. Renaming a file to `.bin` does not change its format.
4. Click **Upload**. Compare the displayed file size and MD5 with your local
   file and wait for the upload to finish. Image-format validation does not
   establish that the model or hardware revision is correct.
5. Click **Update** after the page confirms the upload. Wait for successful
   completion and the router's automatic restart. Keep power connected
   throughout the write; do not restart or submit another update while flashing.
6. Restore the PC's normal network settings when finished.

The Web implementation checks the FIP header and writes the `fip` target
through the model's storage writer, which reads back the written data for
verification. The `/uboot.html` operation does **not** write BL2. This is
why the file to upload here is the complete `fip.bin`, despite the page
being named "U-Boot update".

## Install Through the Serial Console and TFTP

Use this method for BL2 or FIP installation through the console.

### Tools and Connection

Place the model's `fip.bin` and `bl2.img` in the PC's TFTP root
(`192.168.1.2/24`, using the filenames in the table below), then connect the
serial console at **115200 baud, 8N1, no flow control**, interrupt autoboot,
and run the corresponding `mtkupgrade` command at the U-Boot prompt.

### Check the Existing Bootloader and Storage

At the U-Boot prompt, run:

```text
version
help mtkupgrade
printenv ipaddr serverip
mtd list
```

Confirm that named `bl2` and `fip` partitions match the NAND layout above.
The commands below require an existing working bootloader with the MediaTek
`mtkupgrade` command and the stated storage layout. They are not a BootROM
recovery procedure for a device whose bootloader no longer runs.

### Choose the Operation

Run **one** of the following interactive commands at the U-Boot prompt:

| Operation | Command | TFTP filename to enter |
| --- | --- | --- |
| Install the complete FIP (BL31 and U-Boot). | `mtkupgrade fip` | `fip-be10000.bin` |
| Install BL2. | `mtkupgrade bl2` | `bl2-be10000.img` |

Select **TFTP client** from the load-method menu. Enter U-Boot address
`192.168.1.1`, server address `192.168.1.2`, mask `255.255.255.0`
if requested, and the filename in the table. For a complete boot-chain
installation, install the matching FIP first, then BL2 from the same TF-A build,
checking success after each operation before rebooting.

The console FIP path may erase the stored U-Boot environment through its
post-upgrade action. Record custom environment values before upgrading; after
reboot, check the defaults and restore only values appropriate for this board.

Wait for `ATF FIP upgrade completed!` or `ATF BL2 upgrade completed!`,
as applicable, and ensure no image
validation, erase, write, or readback-verification error precedes it.
Then run:

```text
reset
```

Preserve the factory/calibration data, operating-system partitions and
boot environment unless the selected operation explicitly resets it.
A partition-table or whole-device image write is not required for these steps.
BL2 replacement is sensitive to DRAM and storage compatibility; do not proceed
after any validation or write failure.


## Source References

- `Uboot-upstream/configs/gl_be10000_defconfig` and the board device tree above.
- `Uboot-upstream/failsafe/failsafe.c` and
  `Uboot-upstream/failsafe/fsdata/uboot.html`: Web upload format and write target.
- `Uboot-upstream/board/mediatek/common/bootmenu_mtd.c` and
  `bootmenu_mtd_common.c`: upgrade targets and storage writes.
- `Uboot-upstream/board/mediatek/common/cmd_mtkupgrade.c` and
  `load_data.c`: interactive upgrade and TFTP prompts.

Copyright and license notices are preserved in the supplied source files,
`Uboot-upstream/Licenses/`, and `atf-20250304-5ea9f9736/license.rst`.
