# HNink for Kindle

HNink is a standalone GTK+ 2 application for a jailbroken ARM touchscreen Kindle. It opens in its own window and has a Kindle Scriptlet launcher. The installer contains both Kindle ARM userspace variants, a static data service, and source.

## Install by USB

1. Unzip the download. Open the **Kindle** folder in the archive.
2. Copy the **hnink** folder to the root of the Kindle’s USB storage. Copy **documents/HNink.sh** into the Kindle’s existing **documents** folder.
3. Eject the Kindle and open **HNink** from the Kindle library. The launcher supplies the library title and portrait cover through Scriptlet metadata. If the Kindle retains an older library thumbnail, try reconnecting and copying the new launcher again. Your jailbreak setup needs to support Scriptlets. If you already use a Kindle terminal, the equivalent launch command is `sh /mnt/us/hnink/launch.sh`.

The included binaries target **kindlehf** and **kindlepw2** class devices. The launcher selects the appropriate ARM variant using the firmware’s ELF loader. GTK+ 2 and the Kindle X11 environment must be present. Legacy Kindle models and other architectures are outside the build targets. Hardware launch and e-ink behavior still need verification on a physical Kindle.

When upgrading from HNreader, HNink moves the existing `hnreader/data` folder into `hnink/data` on first launch. You can remove the old `documents/HNreader.sh` launcher once HNink opens successfully.

## Read

- Choose a feed from the dropdown below the home header. The app opens **Top** from the cache and checks for updates once in the background. The circular arrow at the upper right refreshes the feed. Discussions update automatically and keep the current scroll view when updated comments arrive. HNink leaves Wi-Fi settings alone.
- The gear opens **Settings**. The typography controls let you choose a shared font family and boldness, and adjust the feed, comments, and article text sizes independently. The preview updates as you adjust each size. The font list includes Bookerly and other Kindle families when available. Each feed page fits as many complete story rows as the text size and available space allow.
- Sign in to Hacker News from **Settings** to add stories to your account’s Favorites with the bookmark button. When signed out, the same button toggles a local bookmark in **Saved**. Sign-in uses HTTPS. The Kindle keeps the session cookie in private storage outside USB storage, and signing out removes the stored session.
- Use the chevrons to turn pages. The upper-left arrow goes back one screen or returns to Kindle Home from the feed. Tap anywhere in a story row to open its discussion. Comment counts appear between points and age. In the discussion header, the globe opens the publisher URL on supported Kindle firmware and the book opens the in-app reader. Text-only posts show their original post above the comments.
- Discussions and articles have a scroll-up arrow in the title bar and a scroll-down arrow at the bottom. **Scroll distance** sets how far these arrows and the page buttons move. Choose **1/4**, **1/3**, **1/2** (the default), or **1 page**. The feed moves one page per press.
- The app detects compatible physical page buttons by their key codes. Their direction follows the display when it rotates to upside-down portrait. Choose **Reversed** under **Physical page buttons** to swap directions.
- Comments are shown in Hacker News order with complete text and indented replies. The first three levels are inline. **Read thread** appears where a deeper branch continues. Only comments near the viewport have UI widgets. A discussion scrolls continuously. Comments load in the background, and fresh cached threads are reused. The symbols by an author jump between sibling comments or collapse the branch. Comment links keep the original URL. Controls briefly invert on touch.
- Refresh fetches the visible story page and the next one. Later pages and comment branches fill on demand. Bookmarks, read markers, downloaded articles, and fetched discussions stay on the device. Uncached pages require a connection.
- Publisher reading extracts text from public HTML and plain-text pages. Availability varies with the publisher, especially for sign-in, paywalled and script-rendered pages, as well as PDFs.
- Cache and launch errors are under `hnink/data` on USB storage. Remove `documents/HNink.sh` and the `hnink` directory to uninstall. Keep a copy of `hnink/data` if you want to retain bookmarks.

## Build with the Kindle SDK

The application uses the [KindleModding GTK tutorial](https://kindlemodding.org/kindle-dev/gtk-tutorial/) window naming convention and the [Kindle SDK](https://kindlemodding.org/kindle-dev/kindle-sdk.html) cross file. On a Linux build machine, install Meson, Ninja, Go 1.22 or later, and the Kindle SDK for the target. From the **Source** directory:

```sh
meson setup --cross-file /path/to/meson-crosscompile.txt build
meson compile -C build
go test ./backend
```

Copy the resulting `hnink-ui` over `Kindle/hnink/hnink-ui-armhf` for the kindlehf build or `Kindle/hnink/hnink-ui-armel` for the kindlepw2 build. Copy `hnink-data` into the same folder.

The included GTK UI binaries were cross-linked against the Kindle GTK2 and GLib public ABI using the included import-link tools. They resolve the real libraries from firmware on launch. Source builds can use the official SDK and firmware sysroot. The Go data service uses the official [Hacker News API](https://github.com/HackerNews/API). Account actions connect directly to the Hacker News website. Signing in is optional.

The included Mozilla CA roots came from the cURL Mozilla bundle distributed by Homebrew. See **Kindle/hnink/CA-NOTICE.txt**.
