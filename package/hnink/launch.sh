#!/bin/sh
set -u
app=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Data lives outside the app folder, so replacing the app (USB copy or a KPM upgrade) never touches it.
if [ -n "${HNINK_DATA_DIR:-}" ]; then
    cache="$HNINK_DATA_DIR"
elif [ -d /mnt/us ]; then
    cache=/mnt/us/hnink-data
else
    cache="$app/data"
fi
mkdir -p "$cache"
exec >>"$cache/launch.log" 2>&1
echo "--- $(date) HNink launch"

# Problems go on the Kindle screen as well as in the log, so a tap never silently does nothing.
fail() {
    echo "$1"
    if command -v eips >/dev/null 2>&1; then
        eips 1 2 "HNink: $1" || true
        eips 1 3 "Details: $cache/launch.log" || true
    fi
    exit 1
}

export DISPLAY="${DISPLAY:-:0.0}"
export SSL_CERT_FILE="$app/ca-certificates.pem"
export HNINK_SESSION_DIR="${HNINK_SESSION_DIR:-/var/local/hnink}"

if [ -x /lib/ld-linux-armhf.so.3 ]; then
    ui="$app/hnink-ui-armhf"
elif [ -x /lib/ld-linux.so.3 ]; then
    ui="$app/hnink-ui-armel"
else
    fail "this Kindle model isn't supported."
fi

# Only a hint: the UI's own exit status decides whether GTK actually works.
hint="the app stopped unexpectedly"
if [ ! -e /usr/lib/libgtk-x11-2.0.so.0 ] && [ ! -e /lib/libgtk-x11-2.0.so.0 ]; then
    echo "GTK2 library not found in /usr/lib or /lib."
    hint="GTK2 seems missing from this firmware"
fi

version=$("$app/hnink-data" --version 2>&1) ||
    fail "the data service can't run on this firmware (kernel $(uname -r))."
echo "$version, kernel $(uname -r), UI $(basename "$ui")"

exit_status=0
"$ui" "$app/hnink-data" "$cache" || exit_status=$?
if [ "$exit_status" -eq 10 ]; then
    lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home || true
    exit 0
fi
[ "$exit_status" -eq 0 ] || fail "$hint (code $exit_status)."
exit 0
