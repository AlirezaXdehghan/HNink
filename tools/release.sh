#!/bin/sh
# Builds the USB download and the KPM package for one version: tools/release.sh VERSION [OUTDIR]
# Needs Go 1.23, python3, clang, arm-none-eabi binutils and zip. The CI workflows install them.
set -eu
version=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${2:-$root/dist}
rm -rf "$out"
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
cd "$root"

# The USB download mirrors the Kindle drive: copy Kindle/ onto it and merge.
kindle="$out/Kindle"
cp -R package "$kindle"
sed -i "s/@VERSION@/$version/" "$kindle/extensions/hnink/config.xml"

go test ./backend
CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 \
    go build -trimpath -ldflags "-s -w -X main.version=$version" -o "$kindle/hnink/hnink-data" ./backend
HNINK_VERSION=$version python3 tools/link-arm.py hard "$kindle/hnink/hnink-ui-armhf"
HNINK_VERSION=$version python3 tools/link-arm.py softfp "$kindle/hnink/hnink-ui-armel"
chmod 755 "$kindle/hnink/hnink-data" "$kindle/hnink/hnink-ui-armhf" "$kindle/hnink/hnink-ui-armel"
(cd "$out" && zip -qr "HNink-$version.zip" Kindle)

# KPM: the app folder is the package root, so our launch.sh doubles as KPM's launch hook.
# KPM versions are three numbers, so a pre-release suffix is dropped here.
numbers=${version%%[-+]*}
IFS=. read -r major minor patch <<EOF
$numbers
EOF
kpm="$out/kpm"
cp -R "$kindle/hnink" "$kpm"
cp kpm/install.sh kpm/uninstall.sh "$kpm/"
cat > "$kpm/manifest.json" <<EOF
{
  "manifest_version": 2,
  "id": "hnink",
  "name": "HNink",
  "author": "AlirezaXdehghan",
  "description": "Hacker News for your Kindle.",
  "version": [$major, $minor, $patch],
  "dependencies": [],
  "supported_platforms": ["kindlepw2", "kindlehf"]
}
EOF
(cd "$kpm" && tar -czf "$out/hnink_${numbers}_kindlepw2-kindlehf.kpkg" *)
rm -rf "$kpm"
echo "Built $out/HNink-$version.zip and $out/hnink_${numbers}_kindlepw2-kindlehf.kpkg"
