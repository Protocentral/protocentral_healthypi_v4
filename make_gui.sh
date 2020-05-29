#!/bin/bash

processing-java --sketch=$(pwd)/gui  --export --platform=linux
cd gui

rm -r healthypi-rpi.zip

zip -r healthypi-rpi.zip application.linux-armv6hf

rm -r application.linux-armv6hf
rm -r application.linux-arm64
rm -r application.linux32
rm -r application.linux64