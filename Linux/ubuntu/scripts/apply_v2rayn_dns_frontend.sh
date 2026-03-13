#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run with sudo: sudo bash $0" >&2
  exit 1
fi

SERVICE_PATH="/etc/systemd/system/v2rayn-local-dns.service"
CONFIG_PATH="/etc/v2rayn-local-dnsmasq.conf"
RESOLV_PATH="/etc/resolv.conf"
BACKUP_PATH="/etc/resolv.conf.v2rayn-backup"
BACKUP_LINK_PATH="/etc/resolv.conf.v2rayn-backup-link"

cat >"${SERVICE_PATH}" <<'EOF'
[Unit]
Description=Local dnsmasq front-end for v2rayN sing-box TUN DNS
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/sbin/dnsmasq --keep-in-foreground --conf-file=/etc/v2rayn-local-dnsmasq.conf
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

cat >"${CONFIG_PATH}" <<'EOF'
port=53
listen-address=127.0.0.1
bind-interfaces
no-resolv
server=172.18.0.2
cache-size=1000
edns-packet-max=1232
clear-on-reload
EOF

if [[ ! -e "${BACKUP_PATH}" ]]; then
  if [[ -L "${RESOLV_PATH}" ]]; then
    cp -a "${RESOLV_PATH}" "${BACKUP_LINK_PATH}"
    cp -aL "${RESOLV_PATH}" "${BACKUP_PATH}"
  else
    cp -a "${RESOLV_PATH}" "${BACKUP_PATH}"
  fi
fi

rm -f "${RESOLV_PATH}"
cat >"${RESOLV_PATH}" <<'EOF'
nameserver 127.0.0.1
options timeout:2 attempts:2
EOF

systemctl daemon-reload
systemctl enable --now v2rayn-local-dns.service

echo "Applied local DNS front-end."
echo "Backups:"
echo "  ${BACKUP_PATH}"
if [[ -e "${BACKUP_LINK_PATH}" ]]; then
  echo "  ${BACKUP_LINK_PATH}"
fi
echo
echo "Quick checks:"
echo "  systemctl status v2rayn-local-dns --no-pager"
echo "  getent ahosts pypi.org"
