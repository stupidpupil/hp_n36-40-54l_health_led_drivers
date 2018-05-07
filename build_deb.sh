#!/usr/bin/fakeroot /bin/sh

PACKAGE_DIR="$1"

PACKAGE_VERSION=$(sed -n '/^PACKAGE_VERSION=\(.*\)$/s//\1/p' "src/$PACKAGE_DIR/dkms.conf")

mkdir -p "tmp/usr/src/"
cp -r "src/$PACKAGE_DIR" "tmp/usr/src/$PACKAGE_DIR-$PACKAGE_VERSION"
mv "tmp/usr/src/$PACKAGE_DIR-$PACKAGE_VERSION/DEBIAN" "tmp/DEBIAN"
echo "Version: $PACKAGE_VERSION" >> "tmp/DEBIAN/control"

# This is pretty horrible!

DEBIAN_SCRIPT_VARS="
DKMS_NAME=$PACKAGE_DIR
DKMS_PACKAGE_NAME=$PACKAGE_DIR-dkms
DKMS_VERSION=$PACKAGE_VERSION
"

cat "src/debian_scripts/header" > "tmp/DEBIAN/postinst"
echo "$DEBIAN_SCRIPT_VARS" >> "tmp/DEBIAN/postinst"
cat "src/debian_scripts/postinst" >> "tmp/DEBIAN/postinst"
chmod +x "tmp/DEBIAN/postinst"

cat "src/debian_scripts/header" > "tmp/DEBIAN/prerm"
echo "$DEBIAN_SCRIPT_VARS" >> "tmp/DEBIAN/prerm"
cat "src/debian_scripts/prerm" >> "tmp/DEBIAN/prerm"
chmod +x "tmp/DEBIAN/prerm"


chmod u=rwx tmp/usr/src/"$PACKAGE_DIR-$PACKAGE_VERSION"/*
chmod og=rx tmp/usr/src/"$PACKAGE_DIR-$PACKAGE_VERSION"/*

dpkg-deb -b tmp dist