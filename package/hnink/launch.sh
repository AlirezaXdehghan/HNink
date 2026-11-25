#!/bin/sh
set -eu
app=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cache="$app/data"
legacy="$(dirname -- "$app")/hnreader/data"
if [ ! -e "$cache" ] && [ -d "$legacy" ]; then
    mv "$legacy" "$cache" || cache="$legacy"
fi
mkdir -p "$cache"
exec >>"$cache/launch.log" 2>&1

export DISPLAY="${DISPLAY:-:0.0}"
export SSL_CERT_FILE="$app/ca-certificates.pem"
export HNINK_SESSION_DIR="${HNINK_SESSION_DIR:-/var/local/hnink}"

if [ -x /lib/ld-linux-armhf.so.3 ]; then
    ui="$app/hnink-ui-armhf"
elif [ -x /lib/ld-linux.so.3 ]; then
    ui="$app/hnink-ui-armel"
else
    echo "HNink needs a supported ARM Kindle with the GTK2/X11 firmware libraries."
    exit 1
fi

exit_status=0
"$ui" "$app/hnink-data" "$cache" || exit_status=$?
if [ "$exit_status" -eq 10 ]; then
    lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home || true
    exit 0
fi
exit "$exit_status"
