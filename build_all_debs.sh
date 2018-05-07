#!/bin/sh

rm -rf "tmp/DEBIAN"
rm -rf "tmp/usr"
./build_deb.sh "gpio-sb8xx"

rm -rf "tmp/DEBIAN"
rm -rf "tmp/usr"
./build_deb.sh "i2c-piix4"

rm -rf "tmp/DEBIAN"
rm -rf "tmp/usr"
./build_deb.sh "softpwm"