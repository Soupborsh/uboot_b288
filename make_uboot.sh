#!/bin/bash

CHIP=sun8iw10p1
CROSS_COMPILE=${CROSS_COMPILE:-/mnt/mediafiles/Media/Code/SDK/gcc/gcc-linaro-4.9-2015.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-}
KERNEL=$PWD/../kernel-b288
SYSCONFIG=sunxi_pack/sys_config.fex
REF_BOOT0=sunxi_pack/boot0_reference
OUT_BOOT0=boot0.img
OUT_UBOOT=u-boot.img

DTC_COMPILER=$KERNEL/scripts/dtc/dtc
DTC_SRC_PATH=$KERNEL/arch/arm/boot/dts/
DTC_DEP_FILE=$KERNEL/arch/arm/boot/dts/.${CHIP}-soc.dtb.d.dtc.tmp
DTC_SRC_FILE=$KERNEL/arch/arm/boot/dts/.${CHIP}-soc.dtb.dts

if [ ! -f ${CROSS_COMPILE}gcc ] ; then
	echo Cross compiler not found. Please check CROSS_COMPILE variable in build script
  exit 1
fi
if [ ! -d $KERNEL ] ; then
	echo Kernel source tree not found. Please check KERNEL variable in build script
  exit 1
fi

# rebuild boot0 and u-boot 

make distclean
make ${CHIP}_config                       || exit 1
make CROSS_COMPILE=${CROSS_COMPILE} boot0 || exit 1
make CROSS_COMPILE=${CROSS_COMPILE}       || exit 1

#build various sunxi tools

cd sunxi_pack/sources || exit 1
make                  || exit 1
cd ..

# create .sys_config.bin from text file

cp -f ../$SYSCONFIG .sys_config.tmp  || exit 1
./script .sys_config.tmp >/dev/null  || exit 1

# create modified text config for include into dtb image

cp -f ../$SYSCONFIG .sys_config.tmp                           || exit 1
sed -i "s/\(\[dram\)_para\(\]\)/\1\2/g" .sys_config.tmp       || exit 1
sed -i "s/\(\[nand[0-9]\)_para\(\]\)/\1\2/g" .sys_config.tmp  || exit 1

# copy header from reference boot0 to our (no other ways known...)
# then update its checksum

cp ../sunxi_spl/boot0/boot0_sdcard.bin ../$OUT_BOOT0             || exit 1
dd if=../$REF_BOOT0 of=../$OUT_BOOT0 bs=1 count=816 conv=notrunc || exit 1
./update_boot0 ../$OUT_BOOT0 .sys_config.bin sdmmc_card          || exit 1

# update u-boot header with config parameters

cp ../u-boot.bin .u-boot.bin                         || exit 1
./update_uboot -no_merge .u-boot.bin .sys_config.bin || exit 1

# compile dtb image

$DTC_COMPILER -O dtb -o .${CHIP}.dtb -b 0 -i $DTC_SRC_PATH -F .sys_config.tmp -d $DTC_DEP_FILE $DTC_SRC_FILE >/dev/null || exit 1

# create boot package understood by boot0

cat >.boot_package.cfg <<EOF
[package]
item=u-boot,                 .u-boot.bin
item=soc-cfg,                .sys_config.bin
item=dtb,                    .${CHIP}.dtb
EOF

./dragonsecboot -pack .boot_package.cfg || exit 1
mv boot_package.fex ../$OUT_UBOOT || exit 1

rm -f .* 2>/dev/null
exit 0

# dd if=u-boot.img of=/dev/mmcblk0 bs=512 seek=24576
# dd if=u-boot.img of=/dev/mmcblk0 bs=512 seek=32800

