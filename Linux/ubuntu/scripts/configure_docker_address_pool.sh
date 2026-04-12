#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run with sudo: sudo bash $0" >&2
  exit 1
fi

DAEMON_JSON="/etc/docker/daemon.json"
BACKUP_DIR="/etc/docker/daemon.json.backups"
TMP_JSON="$(mktemp)"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"

install -d -m 0755 /etc/docker "${BACKUP_DIR}"

if [[ -e "${DAEMON_JSON}" ]]; then
  cp -a "${DAEMON_JSON}" "${BACKUP_DIR}/daemon.json.${TIMESTAMP}"
fi

python3 - "${DAEMON_JSON}" "${TMP_JSON}" <<'PY'
import json
import os
import sys

src_path, dst_path = sys.argv[1], sys.argv[2]

data = {}
if os.path.exists(src_path):
    with open(src_path, "r", encoding="utf-8") as fh:
        data = json.load(fh)

data["default-address-pools"] = [
    {"base": "10.200.0.0/16", "size": 24},
    {"base": "10.201.0.0/16", "size": 24},
]

with open(dst_path, "w", encoding="utf-8") as fh:
    json.dump(data, fh, indent=2, sort_keys=True)
    fh.write("\n")
PY

install -m 0644 "${TMP_JSON}" "${DAEMON_JSON}"
rm -f "${TMP_JSON}"

systemctl restart docker

echo "Configured Docker default-address-pools."
echo "daemon.json: ${DAEMON_JSON}"
if compgen -G "${BACKUP_DIR}/daemon.json.${TIMESTAMP}" >/dev/null; then
  echo "backup: ${BACKUP_DIR}/daemon.json.${TIMESTAMP}"
fi
echo
echo "Quick checks:"
echo "  docker network ls"
echo "  docker network inspect bridge"
echo "  cat ${DAEMON_JSON}"
echo
echo "Note:"
echo "  Existing custom bridge networks keep their old subnets until the owning compose stack is recreated."
