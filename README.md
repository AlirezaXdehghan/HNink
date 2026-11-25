# HNink

Hacker News for a jailbroken Kindle. It turns pages like a book, checks for news once when you open it, and leaves the network alone after that.

Stories and comments come from the official [Hacker News API](https://github.com/HackerNews/API).

## Install

You need a jailbroken touchscreen Kindle, Paperwhite 2 or newer, with Scriptlets or KUAL.

1. Unzip the download.
2. Copy everything inside the **Kindle** folder onto the Kindle drive, merging folders.
3. Eject, then open **HNink** from the library or from KUAL.

If it won't start, the reason shows on screen, and the details are in `hnink-data/launch.log`.

Your saved stories and settings live in `hnink-data/`, so updating HNink never touches them. To uninstall, delete `hnink/`, `documents/HNink.sh` and `extensions/hnink/`, plus `hnink-data/` if you don't want to keep your data.

## Build

On Linux, with Meson, Ninja, Go and the [Kindle SDK](https://kindlemodding.org/kindle-dev/kindle-sdk.html):

```sh
meson setup --cross-file /path/to/meson-crosscompile.txt build
meson compile -C build
go test ./backend
```

Copy `hnink-ui` to `Kindle/hnink/hnink-ui-armhf` for firmware 5.16.3 and later, or to `hnink-ui-armel` for older firmware. Put `hnink-data` next to it. No SDK? `tools/link-arm.py` links the UI against the firmware's own GTK at launch.

Go stays pinned at 1.23. Newer releases need a kernel most Kindles don't have.

`sh tools/release.sh VERSION` builds both downloads without the SDK: the USB zip and the KPM package.

## Release

The version lives in `meson.build`. Push a matching tag and CI publishes it: `v1.0.0-beta.1` becomes a pre-release with the USB zip, and `v1.0.0` is stable and also carries the KPM package.

The CA roots come from cURL's Mozilla bundle; see `Kindle/hnink/CA-NOTICE.txt`.
