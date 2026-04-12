#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run with sudo: sudo bash $0" >&2
  exit 1
fi

SERVICE_PATH="/etc/systemd/system/v2rayn-local-dns.service"
SYNC_SERVICE_PATH="/etc/systemd/system/v2rayn-local-dns-sync.service"
SYNC_TIMER_PATH="/etc/systemd/system/v2rayn-local-dns-sync.timer"
CONFIG_PATH="/etc/v2rayn-local-dnsmasq.conf"
SYNC_SCRIPT_PATH="/usr/local/libexec/v2rayn-local-dns-sync.sh"
RESOLV_PATH="/etc/resolv.conf"
BACKUP_PATH="/etc/resolv.conf.v2rayn-backup"
BACKUP_LINK_PATH="/etc/resolv.conf.v2rayn-backup-link"

install -d -m 0755 /usr/local/libexec

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

cat >"${SYNC_SCRIPT_PATH}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

RESOLV_PATH="/etc/resolv.conf"
BACKUP_PATH="/etc/resolv.conf.v2rayn-backup"
BACKUP_LINK_PATH="/etc/resolv.conf.v2rayn-backup-link"
TUN_RESOLV_CONF="/run/v2rayn-local-dns.resolv.conf"
DNS_SERVICE="v2rayn-local-dns.service"
STATE_PATH="/run/v2rayn-local-dns.mode"

log() {
  logger -t v2rayn-local-dns-sync -- "$1"
  echo "$1"
}

has_tun() {
  ip link show dev singbox_tun >/dev/null 2>&1
}

ensure_backup() {
  if [[ -e "${BACKUP_PATH}" ]]; then
    return
  fi

  if [[ -L "${RESOLV_PATH}" ]]; then
    cp -a "${RESOLV_PATH}" "${BACKUP_LINK_PATH}"
    cp -aL "${RESOLV_PATH}" "${BACKUP_PATH}"
  elif [[ -e "${RESOLV_PATH}" ]]; then
    cp -a "${RESOLV_PATH}" "${BACKUP_PATH}"
  fi
}

write_tun_resolv() {
  cat >"${TUN_RESOLV_CONF}" <<'EOF_RESOLV'
nameserver 127.0.0.1
options timeout:2 attempts:2
EOF_RESOLV

  if ! cmp -s "${TUN_RESOLV_CONF}" "${RESOLV_PATH}" 2>/dev/null; then
    install -m 0644 "${TUN_RESOLV_CONF}" "${RESOLV_PATH}"
  fi
}

restore_backup_resolv() {
  if [[ -L "${BACKUP_LINK_PATH}" ]]; then
    local target
    target="$(readlink "${BACKUP_LINK_PATH}")"
    rm -f "${RESOLV_PATH}"
    ln -s "${target}" "${RESOLV_PATH}"
    return
  fi

  if [[ -e "${BACKUP_PATH}" ]]; then
    install -m 0644 "${BACKUP_PATH}" "${RESOLV_PATH}"
    return
  fi

  if [[ -e /run/systemd/resolve/stub-resolv.conf ]]; then
    rm -f "${RESOLV_PATH}"
    ln -s /run/systemd/resolve/stub-resolv.conf "${RESOLV_PATH}"
  fi
}

cleanup_stale_routing() {
  local pref

  for pref in 9000 9001 9002 9003 9010; do
    while ip rule show | grep -q "^${pref}:"; do
      ip rule del pref "${pref}" || break
    done
  done

  ip route flush table 2022 >/dev/null 2>&1 || true
  ip -6 route flush table 2022 >/dev/null 2>&1 || true
}

sync_tun_mode() {
  ensure_backup
  write_tun_resolv
  systemctl start "${DNS_SERVICE}"
}

sync_direct_mode() {
  systemctl stop "${DNS_SERVICE}" >/dev/null 2>&1 || true
  restore_backup_resolv
  cleanup_stale_routing
}

print_status() {
  echo "tun_present=$(has_tun && echo yes || echo no)"
  echo "resolv_conf=$(readlink -f "${RESOLV_PATH}" 2>/dev/null || echo "${RESOLV_PATH}")"
  systemctl is-active "${DNS_SERVICE}" || true
  ip route show table 2022 || true
  ip rule show | grep -E '^(9000|9001|9002|9003|9010):' || true
}

state_is() {
  local wanted="$1"

  [[ -e "${STATE_PATH}" ]] && [[ "$(<"${STATE_PATH}")" == "${wanted}" ]]
}

cmd="${1:-sync}"
case "${cmd}" in
  sync)
    if has_tun; then
      sync_tun_mode
      if ! state_is "tun"; then
        log "TUN present: keep resolv.conf on 127.0.0.1 and forward DNS to sing-box."
      fi
      printf '%s\n' "tun" >"${STATE_PATH}"
    else
      sync_direct_mode
      if ! state_is "direct"; then
        log "TUN absent: restored backup resolv.conf and cleaned stale sing-box policy routes."
      fi
      printf '%s\n' "direct" >"${STATE_PATH}"
    fi
    ;;
  status)
    print_status
    ;;
  cleanup-stale-routing)
    cleanup_stale_routing
    ;;
  *)
    echo "Usage: $0 [sync|status|cleanup-stale-routing]" >&2
    exit 2
    ;;
esac
EOF
chmod 0755 "${SYNC_SCRIPT_PATH}"

cat >"${SYNC_SERVICE_PATH}" <<'EOF'
[Unit]
Description=Synchronize local DNS front-end with v2rayN sing-box TUN state
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/usr/local/libexec/v2rayn-local-dns-sync.sh sync
EOF

cat >"${SYNC_TIMER_PATH}" <<'EOF'
[Unit]
Description=Poll v2rayN sing-box TUN state and keep host DNS/routing in sync

[Timer]
OnBootSec=10s
OnUnitActiveSec=5s
AccuracySec=1s
Persistent=true

[Install]
WantedBy=timers.target
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

systemctl daemon-reload
systemctl enable --now v2rayn-local-dns-sync.timer
systemctl start v2rayn-local-dns-sync.service

echo "Applied managed local DNS front-end."
echo "Backups:"
echo "  ${BACKUP_PATH}"
if [[ -e "${BACKUP_LINK_PATH}" ]]; then
  echo "  ${BACKUP_LINK_PATH}"
fi
echo
echo "Installed units:"
echo "  ${SERVICE_PATH}"
echo "  ${SYNC_SERVICE_PATH}"
echo "  ${SYNC_TIMER_PATH}"
echo
echo "Quick checks:"
echo "  systemctl status v2rayn-local-dns --no-pager"
echo "  systemctl status v2rayn-local-dns-sync.timer --no-pager"
echo "  sudo /usr/local/libexec/v2rayn-local-dns-sync.sh status"
