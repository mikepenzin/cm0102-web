# The web app with its files, as web/deploy/stage.mjs stages them. Built by `fly deploy` in the stage folder.
# The server only hands out files: the game runs in each player's browser.
FROM node:22-slim
WORKDIR /app
# The stage verified every game file against tools/clean_files/manifest.tsv, so the start skips that (sizes stay checked).
ENV HOST=0.0.0.0 PORT=8080 CM_GAME_DIR=/app/game CM_WEB_BUILD=/app/build-web CM_WEB_SKIP_VERIFY=1
EXPOSE 8080
# One layer per part, from what never changes to what changes most. A deploy rebuilds and uploads a layer only when
# its files changed: a change to the page sends the last layer (KB), a new engine the one before it, and the game
# files (about 500 MB with their brotli copies) go up once.
COPY game/ /app/game/
COPY tools/ /app/tools/
COPY build-web/ /app/build-web/
COPY web/ /app/web/
USER node
CMD ["node", "web/server.mjs"]
