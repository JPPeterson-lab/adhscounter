#!/bin/bash
# adhscounter Version Bump
# Verwendung: bash bump_version.sh 0.1.0 0.2.0

OLD=$1
NEW=$2

if [ -z "$OLD" ] || [ -z "$NEW" ]; then
  echo "Verwendung: bash bump_version.sh <alte-version> <neue-version>"
  echo "Beispiel:   bash bump_version.sh 0.1.0 0.2.0"
  exit 1
fi

echo "Bumpe $OLD -> $NEW ..."

# Shields.io braucht -- statt - vor Suffixen wie "beta"
OLD_BADGE=$(echo "$OLD" | sed 's/-/--/g')
NEW_BADGE=$(echo "$NEW" | sed 's/-/--/g')

# adhscounter.ino
sed -i '' "s/FIRMWARE_VERSION \"$OLD\"/FIRMWARE_VERSION \"$NEW\"/" adhscounter.ino

# docs/
sed -i '' "s/$OLD/$NEW/" docs/version.json
sed -i '' "s/$OLD/$NEW/" docs/manifest.json

# README.md Versions-Badge
sed -i '' "s/Version-v[0-9][^-]*\(--[a-z0-9]*\)*-blue/Version-v${NEW_BADGE}-blue/" README.md

echo "Ergebnis:"
grep "FIRMWARE_VERSION" adhscounter.ino
grep "version" docs/version.json
grep "Version-v" README.md

echo "Fertig. Danach: firmware.bin (Arduino IDE Export) nach docs/firmware/ legen, committen und pushen."
