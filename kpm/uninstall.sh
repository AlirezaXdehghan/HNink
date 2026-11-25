#!/bin/sh
# KPM deletes the package files itself; we only remove the scriptlet. install.sh puts it back
# after an upgrade. Saved stories and settings in /mnt/us/hnink-data are always left alone.
rm -f /mnt/us/documents/HNink.sh
exit 0
