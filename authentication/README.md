# NextGen Tweaks — Licensing & Discord Bot

A self-hosted, cryptographically-secure licensing system for the NextGen Tweaks
desktop app, with a Discord bot for issuing keys. Every part shares one core
library (`libnglicense`) and one SQLite database, so a key you generate in
Discord is immediately redeemable in the app.

```
authentication/
├── libnglicense/      shared C++ core: Ed25519 keys, codec, SQLite store, service
├── cli/               ngtlicense — keygen / issue / revoke / verify (ground truth)
├── server/            ngtauthd  — HTTP validation + account API the app calls
├── bot/               nextgen_bot — Discord bot (D++), issues keys via slash commands
└── data/              issuer keys + licenses.db  (created at runtime — keep private!)
```

## How the security works

* Licence keys are **Ed25519-signed tokens** (`NGTL-…`). The private seed lives
  only on your server/bot; the desktop app embeds just the **public key**. The
  app can therefore prove a key is genuine offline, but **nobody can forge one**
  without your private seed.
* Time limits are signed into the key. **Use counts, machine binding, expiry and
  revocation** are enforced by the server against the database — the single
  online source of truth. Use counting can't be done securely offline, which is
  why the app validates through `ngtauthd`.
* Account passwords are stored as **PBKDF2-HMAC-SHA256** (200k iterations, random
  salt). The server hands the app a short **HMAC-signed session token** so
  credentials are only sent at sign-in.

## Build

Everything uses the same toolchain as the app (OpenSSL + SQLite ship with it):

```bash
OPT=/c/Qt/Tools/mingw1310_64/opt
cmake -S authentication -B authentication/build -G Ninja \
      -DCMAKE_PREFIX_PATH="$OPT" -DOPENSSL_ROOT_DIR="$OPT"
cmake --build authentication/build       # builds nglicense, ngtlicense, ngtauthd, selftest
```

The Discord bot is a separate project (it downloads D++, a few minutes first time):

```bash
MGW=/c/Qt/Tools/mingw1310_64/x86_64-w64-mingw32
cmake -S authentication/bot -B authentication/bot/build -G Ninja \
      -DCMAKE_PREFIX_PATH="$OPT" -DOPENSSL_ROOT_DIR="$OPT" \
      -DZLIB_LIBRARY="$MGW/lib/libz.a" -DZLIB_INCLUDE_DIR="$MGW/include"
cmake --build authentication/bot/build
```

## First-time setup

```bash
cd authentication
./build/ngtlicense keygen            # creates data/issuer_key.bin (SECRET) + issuer_public.bin
./build/ngtlicense pubkey-header > ../src/LicensePublicKey.h   # embed public key in the app
```

Rebuild the desktop app after regenerating the header. **Back up
`data/issuer_key.bin` and never ship it** — losing it invalidates every key you
have issued; leaking it lets anyone mint keys.

## Issuing keys

**From the CLI**

```bash
./build/ngtlicense issue --duration 30d --note "customer A"
./build/ngtlicense issue --uses 25
./build/ngtlicense info   <keyId>
./build/ngtlicense list   [discordUserId]
./build/ngtlicense revoke <keyId>
```

**From Discord** (same result, same database)

```
/license create duration:30d for:@user note:...
/license create uses:25
/license create lifetime:true
/license info   keyid:<hex>
/license list   [for:@user]
/license bind   keyid:<hex> user:@user
/license revoke keyid:<hex>
```

Set the bot up with:

```bash
export DISCORD_TOKEN=...            # bot token from the Discord developer portal
export NGT_DATA_DIR=/path/to/authentication/data
export NGT_GUILD_ID=...             # optional: register commands instantly to one guild
export NGT_ADMIN_ROLE=...           # optional: role id allowed to run commands
./build/nextgen_bot
```

Commands are limited to members with **Manage Server** (or the `NGT_ADMIN_ROLE`).
Generated keys are sent **ephemerally** (only the admin sees them), and when `for:@user` is given the bot also **DMs the key** straight to that member.

## Running the validation server

```bash
export NGT_DATA_DIR=/path/to/authentication/data
export NGT_PORT=8787
./build/server/ngtauthd
```

It speaks plain HTTP; **put a TLS-terminating reverse proxy in front of it**
(Caddy, Cloudflare Tunnel, nginx) for production. Point the desktop app at it
with the `NGT_AUTH_URL` environment variable (default `http://127.0.0.1:8787`).

### API

| Method | Path | Body | Purpose |
|--------|------|------|---------|
| GET  | `/v1/health`           | –                                                   | liveness + public key |
| POST | `/v1/account/create`   | `username,email,password,license,machine`           | register + bind licence |
| POST | `/v1/account/signin`   | `identifier,password,license?,machine`              | authenticate + validate (consumes a use) |
| POST | `/v1/session/validate` | `token,machine`                                     | silent re-validation (no use consumed) |

## In the app

* **Sign In** takes username/email + password + an optional licence key (a
  returning user whose key is already bound doesn't retype it).
* **Create Account** opens a dialog that requires the licence key from Discord.
* The app verifies the key's signature offline first, then calls the server to
  bind and enforce it. A saved session lets it skip the password on next launch.
