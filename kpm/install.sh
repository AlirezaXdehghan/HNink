#!/bin/sh
# KPM runs this from the unpacked package folder, on install and again after every upgrade.
# Packages place their own scriptlet; it hands launching back to KPM, which runs our launch.sh.
set -eu
here=$(pwd)
cat > /mnt/us/documents/HNink.sh <<EOF
#!/bin/sh
# Name: HNink
# Author: AlirezaXdehghan
# Icon: $here/cover.jpg

exec /var/local/kmc/bin/kpm launch hnink
EOF
