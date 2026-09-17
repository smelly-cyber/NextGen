# NextGen Tweaks backend - one image that runs BOTH the licence/validation
# server (ngtauthd) and the Discord bot (nextgen_bot). They must live together
# because they share one SQLite database file, and a Render persistent disk can
# only attach to a single service.
#
# IMPORTANT: the build context is the REPOSITORY ROOT (the folder that contains
# both `authentication/` and `assets/`), because the bot draws the logo from
# assets/. In Render this is set by dockerContext in render.yaml.
#
#   docker build -f authentication/deploy/Dockerfile -t nextgen-backend .

# ---- Build stage -----------------------------------------------------------
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates pkg-config \
        libssl-dev libsqlite3-dev zlib1g-dev libopus-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY authentication /src

# Licence core + validation server + CLI (the CLI generates the issuer key pair
# on first boot).
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

# Discord bot (fetches and builds D++ on the way - this is the slow step).
RUN cmake -S bot -B bot/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build bot/build -j"$(nproc)"

# ---- Runtime stage ---------------------------------------------------------
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates libssl3 libsqlite3-0 zlib1g libopus0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/server/ngtauthd /app/ngtauthd
COPY --from=build /src/build/ngtlicense      /app/ngtlicense
COPY --from=build /src/bot/build/nextgen_bot /app/nextgen_bot
COPY assets                                  /app/assets
COPY authentication/deploy/start.sh          /app/start.sh
RUN chmod +x /app/start.sh

# The persistent disk is mounted here; issuer keys + licenses.db live on it.
ENV NGT_DATA_DIR=/data
# The bot draws the logo into its embeds from here.
ENV NGT_LOGO_PATH=/app/assets/logo/nextgen_tweaks_mark.png

CMD ["/app/start.sh"]
