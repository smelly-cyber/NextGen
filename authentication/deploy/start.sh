#!/usr/bin/env bash
# Boots the NextGen Tweaks backend: generates the issuer key pair on first run,
# starts the Discord bot in the background, then runs the validation server in
# the foreground bound to the port Render gives us.
set -euo pipefail

export NGT_DATA_DIR="${NGT_DATA_DIR:-/data}"
mkdir -p "$NGT_DATA_DIR"

# First boot on a fresh disk: mint the Ed25519 issuer key pair. It is written to
# the persistent disk and reused on every later deploy, so licences keep
# verifying across restarts. Keep issuer_key.bin secret - it can mint licences.
if [ ! -f "$NGT_DATA_DIR/issuer_key.bin" ]; then
  echo "[start] No issuer key found - generating one in $NGT_DATA_DIR"
  /app/ngtlicense keygen
fi

# Render terminates TLS and forwards to $PORT; bind every interface.
export NGT_HOST=0.0.0.0
export NGT_PORT="${PORT:-8787}"

# Start the Discord bot only if a token is configured. It shares the same
# database + issuer key as the server, so /gen, /revoke etc. and the app agree.
if [ -n "${DISCORD_TOKEN:-}" ]; then
  echo "[start] Launching Discord bot"
  /app/nextgen_bot &
else
  echo "[start] DISCORD_TOKEN not set - skipping the Discord bot"
fi

echo "[start] Launching auth server on 0.0.0.0:$NGT_PORT"
exec /app/ngtauthd
