#!/usr/bin/env bash
set -euo pipefail

tun_present="no"
if ip link show dev singbox_tun >/dev/null 2>&1; then
  tun_present="yes"
fi

route_2022="$(ip route show table 2022 || true)"
rules_2022="$(ip rule show | grep -E '^(9000|9001|9002|9003|9010):' || true)"

echo "tun_present=${tun_present}"
echo "table_2022=${route_2022:-<empty>}"
echo "rules_2022=${rules_2022:-<empty>}"

if [[ "${tun_present}" == "no" ]] && [[ -n "${route_2022}${rules_2022}" ]]; then
  echo
  echo "stale_singbox_policy_routing=yes"
  echo "Direct connectivity can be blackholed until these rules are cleaned."
  exit 1
fi

echo
echo "stale_singbox_policy_routing=no"
