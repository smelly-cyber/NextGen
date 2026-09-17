# NextGen Tweaks — Full Setup & Deployment Guide

This is the complete, plain‑English guide to everything that makes NextGen Tweaks
work end to end:

1. What the pieces are and how they fit together
2. Your owner login and the owner licence
3. Hosting the backend (server **and** Discord bot) on [Render](https://render.com)
4. Pointing the desktop app at your server
5. Auto‑updating the app (fully automatic — no URLs, nothing for users to download)
6. Packaging the app into **one standalone `.exe`** with **Enigma Virtual Box**
7. What the Admin panel and Discord bot can do
8. A simple checklist for releasing a new version

You don't need to be a programmer to follow this. Where a step needs a value from
you (a token, a URL), it says so clearly.

---

## 0. How the pieces fit together

There are three programs:

- **The desktop app** (`NextGenTweaks.exe`) — what your customers run. It signs in,
  checks the licence, applies the tweaks, and (for you) shows the Admin panel.
- **The auth server** (`ngtauthd`) — the online authority for accounts, licences,
  HWID locks, the Admin panel's data, and the client "kill switch"/update signals.
- **The Discord bot** (`nextgen_bot`) — lets you run `/gen`, `/revoke`,
  `/resethwid`, etc. from Discord.

The server and the bot **share one database file** (`licenses.db`) and **one secret
signing key** (`issuer_key.bin`). Because of that they must run **together in one
place** (a licence made in Discord is instantly usable in the app, and vice‑versa).

The desktop app **never touches the database directly** — it only talks to the
server over HTTPS. So the only things that need hosting are the server + bot.

---

## 1. Your owner login (important — read first)

- **Username:** `darcy`
- **Password:** `Autumn2026!`
- **Owner licence key:** `NGT-OWNER-ACCESS`

How the owner works:
- The owner licence is stored **on the server**, never inside the app, so nobody can
  fake owner access by editing the app.
- Any account that types `NGT-OWNER-ACCESS` in the in‑app activation box becomes the
  owner and gets a lifetime licence. You only type it once.
- The owner has **no HWID lock** (you can sign in from any PC / a new PC) and is
  **immune to nuke / disable / reset**. The owner's own licence never shows up in,
  and can never be revoked by, the licence table or the bot.
- To change the owner key later, edit `meta.owner_license` in `licenses.db` (see
  "Advanced" at the end).

Only the owner sees the **Admin** button in the sidebar. A normal licensed user
sees Home → Settings and nothing else.

---

## 2. Host the backend on Render

The server and bot run together in **one Docker web service**. Everything is
pre‑wired — you mainly click buttons and paste a token.

### 2.1 Put the code on GitHub
Push this whole project (the folder with `authentication/`, `assets/`,
`render.yaml`) to a **GitHub repo**. It can be **private**.

### 2.2 Create the service
1. In Render: **New +** → **Blueprint**.
2. Choose your repo. Render reads [`render.yaml`](render.yaml) and proposes one
   service, **nextgen-backend**.
3. Click **Apply**. It will ask for the secret values below.

### 2.3 Fill in the environment variables
On the service's **Environment** tab:

| Key | Needed? | What it is |
|-----|---------|------------|
| `DISCORD_TOKEN` | Yes (for the bot) | Your bot token — Discord Developer Portal → your app → **Bot** → **Reset Token**. |
| `NGT_GUILD_ID` | Optional | A Discord **server ID**. Set it → slash commands appear **instantly** in that server. Blank → they can take up to an hour. |
| `NGT_ADMIN_ROLE` | Optional | A Discord **role ID**. Only that role can run the bot commands. Blank → anyone with "Manage Server". |
| `NGT_LOG_WEBHOOK` | Optional | A Discord webhook the app posts login logs to. |

Everything else (`NGT_DATA_DIR`, `NGT_HOST`, `NGT_PORT`, `NGT_LOGO_PATH`) is set for
you automatically. **Don't add those.**

### 2.4 First deploy
Render builds the image. **The first build takes ~10–20 minutes** (it compiles the
Discord library D++); later builds are cached and fast. When it's live you get a
URL like:

```
https://nextgen-backend.onrender.com
```

Render adds HTTPS for you automatically.

### 2.5 Check it worked
Open `https://nextgen-backend.onrender.com/v1/health`. You should see:
```json
{"ok":true,"service":"ngtauthd","publicKey":"…"}
```

On first boot the container creates your secret keys and database on the disk
(`/data/issuer_key.bin`, `issuer_public.bin`, `licenses.db`). They **survive every
future deploy** because they live on the persistent disk.

### 2.6 Plan note (important)
The blueprint uses the **Starter (paid)** plan on purpose. You need it for:
- a **persistent disk** — Free has none, so your database would wipe on every deploy;
- **24/7 uptime** — Free services sleep when idle, which would drop the Discord bot.

### 2.7 Invite the bot to your Discord
Discord Developer Portal → your app → **OAuth2 → URL Generator** → tick **`bot`** and
**`applications.commands`** → open the generated URL → add it to your server. Once the
service is live it registers the commands and writes a heartbeat, so your in‑app
**System Status → Discord Bot** shows **Online**.

---

## 3. Point the desktop app at your server

The app defaults to `http://127.0.0.1:8787` (local testing). Your released app must
point at your Render URL. Two ways:

**A. Quick (your own testing):** set an environment variable, then restart the app:
```
setx NGT_AUTH_URL https://nextgen-backend.onrender.com
```

**B. Recommended for distribution:** bake the URL into the build so users never touch
anything. Tell me your final Render URL and I'll set the built‑in default in the app
(one place in the code) and rebuild. Then a plain double‑click just works.

---

## 4. Auto‑update — fully automatic

The app keeps itself up to date. **You never paste a URL and users never download
anything** — you just publish a new GitHub Release and every app updates itself.

### How it works
- The app watches **one GitHub repo**, set once in the code:
  `kUpdateRepo` in [`src/Updater.cpp`](src/Updater.cpp) (currently `smelly-cyber/NextGen`).
- On launch and hourly, each app asks GitHub for that repo's **latest release**.
- If the release's version tag (e.g. `v1.0.1`) is **newer** than the running app, it
  downloads the `.exe` attached to that release, verifies it against the
  **SHA‑256 GitHub publishes**, and swaps itself for it on restart — silently.
- Offline or already current → it does nothing and tries again later.

### Releasing a new version
1. Bump the version so apps know it's newer: in [`src/main.cpp`](src/main.cpp) change
   `setApplicationVersion("1.0.0")` to the new number, and build.
2. **Pack it** into one standalone exe with Enigma Virtual Box (Part 5).
3. On GitHub, create a **Release**:
   - **Tag:** `v1.0.1` (match the version you built),
   - **Attach** the packed `NextGenTweaks.exe` as a release **asset**,
   - Publish.
4. Done. Every app updates to `1.0.1` on its own within the hour.

> The release asset must be **publicly downloadable** (no login). Your source repo
> can stay private — Release assets can be public, or use a small separate public
> repo just for releases and point `kUpdateRepo` at it.

The Admin panel's **Push Updates** box still exists as a manual override (push a
version + an optional URL), but with automatic updates you normally don't need it.

---

## 5. Packaging into one standalone `.exe` (Enigma Virtual Box)

The app is built as a normal Windows program, so on its own the `.exe` needs its
support files (Qt DLLs, a platform plugin, the OpenSSL and compiler runtime DLLs)
sitting next to it. Enigma Virtual Box wraps the `.exe` **and** all those files into
**one single `.exe`** that runs anywhere with nothing else beside it.

You do this once per release, after building. Two stages: **gather the files**, then
**bundle them**.

### Stage 1 — Gather the files (one folder with everything)
There's a helper that collects the exact set into a clean `dist\` folder for you.
From a normal terminal in the project root:

```bat
package.bat
```

(If you'd rather do it by hand, `package.bat` just does this: copies the freshly
built `NextGenTweaks.exe` into `dist\`, runs Qt's `windeployqt` on it to pull in the
Qt DLLs + the `platforms\qwindows.dll` plugin, then copies the three MinGW runtime
DLLs — `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` — and the
OpenSSL `libcrypto-1_1-x64.dll`.)

After it runs, `dist\` contains `NextGenTweaks.exe` plus its DLLs and a `platforms\`
folder. **Confirm it works before packing:** double‑click `dist\NextGenTweaks.exe`.
If it opens, the file set is complete.

### Stage 2 — Bundle with Enigma Virtual Box
1. Open **Enigma Virtual Box**.
2. **Enter Input File Name:** browse to `dist\NextGenTweaks.exe`.
3. **Enter Output File Name:** it suggests `dist\NextGenTweaks_boxed.exe` — that's your
   single standalone file. (You can rename it back to `NextGenTweaks.exe` afterwards.)
4. In the big **Files** area, click **Add** → **Add Folder Recursive**, and pick the
   `dist` folder — this pulls in every DLL and the `platforms` folder so they get
   packed inside. (Make sure the `platforms\qwindows.dll` stays under a `platforms`
   sub‑folder in the list — Enigma keeps the structure automatically.)
5. Open **Files Options** and tick:
   - **Virtualize the registration of the system files** (on),
   - **Compress Files** (on) — smaller output,
   - leave the rest at defaults.
6. Click **Process**. When it finishes you have **one** `.exe` in `dist\` that runs on
   any Windows PC with nothing else next to it.
7. Rename that boxed file to `NextGenTweaks.exe`. **This is the file you:**
   - hand to customers, **and**
   - attach to the GitHub Release for auto‑update.

Because the packed exe carries its own matching DLLs inside it, the single‑file
auto‑update stays clean forever — swapping one packed exe swaps everything at once.

> **Tip:** packed exes can occasionally trigger antivirus false positives. If you plan
> to distribute widely, **code‑sign** the packed exe (a code‑signing certificate) —
> it removes most warnings and is good practice for a paid product.

---

## 6. What the Admin panel does (owner only)

- **Stats** — total users, active licences, online now (last 5 min), app version. Live.
- **Remote Management**
  - **Nuke Client / Restore Client** — a reversible kill switch. Nuke makes a client
    **refuse to run** (it closes instantly on launch, nothing shown) until you press
    **Restore Client** (the button turns green), which lets it run again with its data
    intact. Your owner PC is never affected.
  - **Disable Client** — blocks a client at sign‑in.
  - **Reset Client** — clears a client's settings + session (back to sign‑in).
  - **Send Message** — pops a message on the user's app.
- **Push Updates** — optional manual update push (see Part 4).
- **License Management** — search, filter, **Add License** (lifetime / duration / uses),
  per‑row view + revoke, **Revoke Selected**, pagination. HWID shows the bound machine
  once a key is activated (unactivated keys read "Not activated"). Your owner licence is
  never listed here.
- **System Status** — live health of Auth / License / Update / Database / Discord Bot.
- **User Activity** — real feed (sign‑ins, optimisation runs), View All for the rest.
- **Security & Tools** — Clear All Licenses (keeps yours), Export User Data, View Audit
  Logs, Application Settings.

Everything an owner does here, **and** everything done from Discord, is written to the
**Audit Log** with who did it.

---

## 7. Discord bot commands

All reply in blue branded embeds, all require the admin role (if you set one), and all
are recorded in the app's Audit Log as `name (userid)`:

- `/gen` and `/license create` — mint a licence (duration / uses / lifetime).
- `/revoke` and `/license revoke` — revoke a licence (refuses owner licences).
- `/blacklist` / `/unblacklist` — ban/unban a licence key or a machine.
- `/resethwid` — unbind a licence so it can be activated on a new machine/person.
- `/viewlicenses` — list all licences (**never** shows yours).
- `/clearlicenses` — delete every licence (**keeps** yours).
- `/license info` — show a licence's state.

---

## 8. The performance tweaks

The app applies **270+ documented, reversible Windows registry/service changes**,
grouped into the toggles on Optimise / Enhance / Perform. Nothing is applied until a
user runs a tweak, every change is recorded first, and **Settings → Revert System
Changes** puts everything back. (Security‑weakening tweaks like CPU‑mitigation or
Defender toggles are deliberately **not** included.)

---

## 9. Release checklist (the whole flow, short version)

1. Make your changes, bump the version in `src/main.cpp`, build.
2. `package.bat` → confirm `dist\NextGenTweaks.exe` opens.
3. Enigma Virtual Box → **Process** → get one standalone exe → rename to
   `NextGenTweaks.exe`.
4. GitHub → **New Release**, tag `vX.Y.Z`, attach the packed exe, publish.
5. Every existing app auto‑updates within the hour. New customers get the packed exe.

---

## Advanced / reference

**Environment variables**

| Variable | Where | Purpose |
|----------|-------|---------|
| `DISCORD_TOKEN` | Render | Discord bot token |
| `NGT_GUILD_ID` | Render | Instant slash‑command registration in one server |
| `NGT_ADMIN_ROLE` | Render | Role allowed to run bot commands |
| `NGT_LOG_WEBHOOK` | Render | Discord webhook for app login logs |
| `NGT_DATA_DIR` / `NGT_PORT` / `NGT_HOST` / `NGT_LOGO_PATH` | Render (auto) | Set for you |
| `NGT_AUTH_URL` | Client PCs | The app's server URL (your Render URL) |

**Change the owner licence key:** on the Render service, the database is at
`/data/licenses.db`. Update the value of the `meta` row with key `owner_license`
(any SQLite tool). The new key takes effect immediately.

**Change the update repo:** edit `kUpdateRepo` in `src/Updater.cpp` and rebuild.
