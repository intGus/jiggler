#!/usr/bin/env bash
# One-time (per boot) USB HID gadget setup for CardputerZero.
# Creates a composite gadget with:
#   /dev/hidg0  boot keyboard, 8-byte reports  [mod, 0, k0..k5]
#   /dev/hidg1  boot mouse,    4-byte reports  [buttons, dx, dy, wheel]
# Matches src/platform/jiggle_output.cpp. Installed by the .deb and run at boot by
# cp0-hid-gadget.service. Manual run as root:  sudo ./setup-hid-gadget.sh
# Undo:  sudo ./setup-hid-gadget.sh --remove
set -euo pipefail

G=/sys/kernel/config/usb_gadget/cp0hid
# Device nodes go to APP_USER if set, else to group plugdev (default desktop user group).
APP_USER="${APP_USER:-${SUDO_USER:-}}"

remove() {
  [ -d "$G" ] || { echo "no gadget to remove"; return; }
  echo "" > "$G/UDC" 2>/dev/null || true
  rm -f "$G"/configs/c.1/hid.usb0 "$G"/configs/c.1/hid.usb1
  rmdir "$G"/configs/c.1/strings/0x409 "$G"/configs/c.1 2>/dev/null || true
  rmdir "$G"/functions/hid.usb0 "$G"/functions/hid.usb1 2>/dev/null || true
  rmdir "$G"/strings/0x409 "$G" 2>/dev/null || true
  echo "gadget removed"
}

if [ "${1:-}" = "--remove" ]; then remove; exit 0; fi
[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }

modprobe libcomposite
modprobe usb_f_hid 2>/dev/null || true
[ -d "$G" ] && remove

mkdir -p "$G"
cd "$G"
echo 0x1d6b > idVendor        # Linux Foundation
echo 0x0104 > idProduct       # Multifunction Composite Gadget
echo 0x0100 > bcdDevice
echo 0x0200 > bcdUSB
mkdir -p strings/0x409
echo "CP0HID0002"      > strings/0x409/serialnumber
echo "CardputerZero"   > strings/0x409/manufacturer
echo "USB Keyboard and Mouse" > strings/0x409/product

# Keyboard -> hidg0 (created first so it gets minor 0)
mkdir -p functions/hid.usb0
echo 1 > functions/hid.usb0/protocol   # keyboard
echo 1 > functions/hid.usb0/subclass   # boot interface
echo 8 > functions/hid.usb0/report_length
printf '\x05\x01\x09\x06\xa1\x01\x05\x07\x19\xe0\x29\xe7\x15\x00\x25\x01\x75\x01\x95\x08\x81\x02\x95\x01\x75\x08\x81\x03\x95\x05\x75\x01\x05\x08\x19\x01\x29\x05\x91\x02\x95\x01\x75\x03\x91\x03\x95\x06\x75\x08\x15\x00\x25\x65\x05\x07\x19\x00\x29\x65\x81\x00\xc0' \
  > functions/hid.usb0/report_desc

# Mouse -> hidg1: 3 buttons, X, Y, wheel (relative, -127..127)
mkdir -p functions/hid.usb1
echo 2 > functions/hid.usb1/protocol   # mouse
echo 1 > functions/hid.usb1/subclass
echo 4 > functions/hid.usb1/report_length
printf '\x05\x01\x09\x02\xa1\x01\x09\x01\xa1\x00\x05\x09\x19\x01\x29\x03\x15\x00\x25\x01\x95\x03\x75\x01\x81\x02\x95\x01\x75\x05\x81\x03\x05\x01\x09\x30\x09\x31\x09\x38\x15\x81\x25\x7f\x75\x08\x95\x03\x81\x06\xc0\xc0' \
  > functions/hid.usb1/report_desc

mkdir -p configs/c.1/strings/0x409
echo "Keyboard + Mouse" > configs/c.1/strings/0x409/configuration
echo 250 > configs/c.1/MaxPower
ln -s functions/hid.usb0 configs/c.1/
ln -s functions/hid.usb1 configs/c.1/

ls /sys/class/udc | head -n1 > UDC

# Let the app (running as a normal user) write the reports.
for i in 0 1; do
  for _ in $(seq 20); do [ -e /dev/hidg$i ] && break; sleep 0.1; done
  if [ -n "$APP_USER" ]; then chown "$APP_USER" /dev/hidg$i; else chgrp plugdev /dev/hidg$i; fi
  chmod 0660 /dev/hidg$i
done
echo "gadget up on $(cat UDC):"; ls -l /dev/hidg*
