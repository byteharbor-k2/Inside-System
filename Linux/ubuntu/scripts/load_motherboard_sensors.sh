#!/usr/bin/env bash
# Load the motherboard Super I/O sensor driver (fan speeds / board temps)
# and persist it via /etc/modules-load.d/sensors.conf.
#
# Privileged actions: apt install, modprobe, writes to /etc/modules-load.d/.
# Usage: bash Linux/ubuntu/scripts/load_motherboard_sensors.sh
set -euo pipefail

readonly MODULES=(nct6775 it87 w83627ehf f71882fg)
readonly CHIP_PATTERN='nct6[0-9]+|it8[0-9]+|w83627[a-z]*|f71[0-9a-z]+'
readonly PERSIST_FILE="/etc/modules-load.d/sensors.conf"

sudo apt install -y lm-sensors >/dev/null

found=""
for mod in "${MODULES[@]}"; do
    if sudo modprobe "${mod}" 2>/dev/null; then
        sleep 1
        if grep -qxE "${CHIP_PATTERN}" /sys/class/hwmon/hwmon*/name 2>/dev/null; then
            found="${mod}"
            break
        fi
        sudo modprobe -r "${mod}" 2>/dev/null || true
    fi
done

if [[ -n "${found}" ]]; then
    echo "${found}" | sudo tee "${PERSIST_FILE}" >/dev/null
    echo ">>> Loaded '${found}' and set it to load at boot (${PERSIST_FILE})."
    sensors || true
else
    echo ">>> No Super I/O driver matched. Recent kernel messages:"
    sudo dmesg | grep -iE 'nct|it87|resource conflict|ACPI' | tail -10 || true
    echo ">>> An 'ACPI resource conflict' means the BIOS locks the chip;"
    echo ">>> add kernel parameter acpi_enforce_resources=lax and reboot."
    exit 1
fi
