#!/bin/bash
set -e
make qemu
NET=192.168.168.0/24
HOST=192.168.168.1
SOS=192.168.168.2

QEMU=qemu-system-arm

KERNEL=images/sos-image-arm-imx6
#KERNEL=u-boot

CORE_ARGS="-machine sabrelite -nographic -m size=1024M "
SERIAL_ARGS="-serial null -serial mon:stdio -kernel $KERNEL"
GDB_ARGS="-s"

#SD_ARGS="-sd sd.img"

#FLASH_ARGS="-device driver=sst25vf016b,sst25vf016b.file=spiflash.img"
#FLASH_ARGS="-drive file=<file>,if=mtd,index=<index>"

NET_ARGS="-netdev user,id=fec0,net=$NET,host=$HOST"

echo "$QEMU $CORE_ARGS $SERIAL_ARGS $GDB_ARGS $NET_ARGS $SD_ARGS $FLASH_ARGS $REDIR_ARGS $@"
echo ""
$QEMU $CORE_ARGS $SERIAL_ARGS $GDB_ARGS $NET_ARGS $SD_ARGS $FLASH_ARGS $REDIR_ARGS $@



