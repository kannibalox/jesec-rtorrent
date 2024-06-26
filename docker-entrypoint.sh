#!/bin/sh
set -eu
USER_NAME="${USER_NAME:-download}"
GROUP_NAME="${GROUP_NAME:-download}"
USER_HOME="${USER_HOME:-/opt/rtorrent}"
PUID="${PUID:-1000}"
PGID="${PGID:-1000}"
if [ "$(id -u)" = "0" ] && [ -z "${DISABLE_PERM_DROP:-}" ]; then
  if ! getent group "$PGID" >/dev/null; then
    groupadd -g "$PGID" "$GROUP_NAME"
  fi
  if ! getent passwd "$PUID" >/dev/null; then
    useradd -m -d "$USER_HOME" -g "$PGID" -u "$PUID" "$USER_NAME"
  fi
  exec sudo -u "#$PUID" -g "#$PGID" -- "$@"
else
  exec "$@"
fi
