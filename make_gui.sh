#!/bin/bash

processing-java --sketch=$(pwd)/gui  --export --platform=linux
cd gui

rm -r healthypi-rpi.zip

zip -r healthypi-rpi.zip application.linux-armv6hf

rm -r application.linux-armv6hf
rm -r application.linux-arm64
rm -r application.linux32
rm -r application.linux64

# Test code for unzip files

ROOTFS_DIR="/Users/akw/Documents"

unzip healthypi-rpi.zip
mkdir ${ROOTFS_DIR}/HealthyPi
#mv application.linux-armv6hf files

install -v -d "${ROOTFS_DIR}/HealthyPi"
install -v -d "${ROOTFS_DIR}/HealthyPi/lib"
install -v -d "${ROOTFS_DIR}/HealthyPi/data"

for file in files/lib/*;do
    install -m 755 "$file" "${ROOTFS_DIR}/HealthyPi/lib"
done

for file in files/data/*;do
    install -m 755 "$file" "${ROOTFS_DIR}/HealthyPi/data"
done

install -m 777 "files/gui" "${ROOTFS_DIR}/HealthyPi/gui"
#install -v -o 1000 -g 1000 -m 644 "files/gui" "${ROOTFS_DIR}/HealthyPi/"

#install -v -o 1000 -g 1000 -m 644 "files/gui" "${ROOTFS_DIR}/home/${FIRST_USER_NAME}/MagPi/"


