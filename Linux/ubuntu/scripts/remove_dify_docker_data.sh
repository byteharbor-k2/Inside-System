#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run with sudo: sudo bash $0" >&2
  exit 1
fi

TARGET="/home/howie/Workspace/personal/dify/docker/volumes"

rm -rf "${TARGET}"

echo "Removed ${TARGET}"
