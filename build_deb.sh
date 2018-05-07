#!/usr/bin/fakeroot /bin/sh

DEBTREE="tmp/debtree"

PACKAGE_DIR="$1"

PACKAGE_VERSION=$(sed -n '/^PACKAGE_VERSION=\(.*\)$/s//\1/p' "src/$PACKAGE_DIR/dkms.conf")

mkdir -p "$DEBTREE/usr/src/"
cp -r "src/$PACKAGE_DIR" "$DEBTREE/usr/src/$PACKAGE_DIR-$PACKAGE_VERSION"
mv "$DEBTREE/usr/src/$PACKAGE_DIR-$PACKAGE_VERSION/DEBIAN" "$DEBTREE/DEBIAN"
echo "Version: $PACKAGE_VERSION" >> "$DEBTREE/DEBIAN/control"

# This is pretty horrible!

DEBIAN_SCRIPT_VARS="
DKMS_NAME=$PACKAGE_DIR
DKMS_PACKAGE_NAME=$PACKAGE_DIR-dkms
DKMS_VERSION=$PACKAGE_VERSION
"

cat "src/debian_scripts/header" > "$DEBTREE/DEBIAN/postinst"
echo "$DEBIAN_SCRIPT_VARS" >> "$DEBTREE/DEBIAN/postinst"
cat "src/debian_scripts/postinst" >> "$DEBTREE/DEBIAN/postinst"
chmod +x "$DEBTREE/DEBIAN/postinst"

cat "src/debian_scripts/header" > "$DEBTREE/DEBIAN/prerm"
echo "$DEBIAN_SCRIPT_VARS" >> "$DEBTREE/DEBIAN/prerm"
cat "src/debian_scripts/prerm" >> "$DEBTREE/DEBIAN/prerm"
chmod +x "$DEBTREE/DEBIAN/prerm"


chmod u=rwx $DEBTREE/usr/src/"$PACKAGE_DIR-$PACKAGE_VERSION"/*
chmod og=rx $DEBTREE/usr/src/"$PACKAGE_DIR-$PACKAGE_VERSION"/*

dpkg-deb -b $DEBTREE dist