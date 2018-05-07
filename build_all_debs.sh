#!/bin/sh

rm -rf "tmp/debtree"
/usr/bin/fakeroot /bin/sh build_deb.sh "gpio-sb8xx"

rm -rf "tmp/debtree"
/usr/bin/fakeroot /bin/sh build_deb.sh "i2c-piix4"

rm -rf "tmp/debtree"
/usr/bin/fakeroot /bin/sh build_deb.sh "softpwm"