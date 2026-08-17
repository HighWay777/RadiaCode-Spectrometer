# Boiler Controller HTTP API

This document lists the HTTP endpoints exposed by the ESP32 Boiler Controller, the data contracts, authentication options for both browsers and headless remote controllers, and usage guidance.

## Authentication

Two roles: `admin` (full control) and `vic` (limited/read-only scope in UI; both map to isAdmin for API controls today).

Supported auth methods:
- Cookie-based (browser): `session=<token>` (admin) or `session_vic=<token>`.
- Token-based (headless):
  - `Authorization: Bearer <token>`
  - or `X-Session: <token>`

Obtaining a token:
- `POST /login` with form fields `login`, `password` or JSON `{ "login": "...", "password": "..." }`.
- On JSON `Accept`, the response includes `{ status: "ok", token: "...", role: "admin|vic" }`. Store `token` and send it on subsequent requests via `Authorization: Bearer <token>` or `X-Session: <token>`.

Session lifecycle:
- Tokens are also persisted in NVS. `POST /logout` clears the caller's token.
- On device reboot, sessions are cleared by default; clients should re-login to refresh tokens.

Security recommendations:
- Place the device behind a LAN/VPN or a TLS-terminating reverse proxy (Nginx, Caddy, HA); enable Basic Auth at the proxy if exposing beyond LAN.
- Avoid multiple concurrent dashboards; the device enforces rate limiting to protect RAM.

## Core Data

- `GET /api/status` (public, lightweight)
  - Snapshot for UIs. Safe to poll (client-side backoff still recommended).
  - Fields (subset): `currentTemp`, `targetTemp`, `target`, `targetSource`, `isNightTime`, `isBoostTime`, `dayTemp`, `nightTemp`, `boostTemp`, `heater`, `heaterOn`, `manualOverride`, `heaterControlMode`, `aiEnabled`, `aiHasEnoughData`, `aiOptimizationCycles`, `efficiency` (percent), `heatLossRate`, `uptime`, `wifi`, `wifiRSSI`, memory stats, history meta, `serverTime`, `timezone`.
  - Example (PowerShell):
    - `curl http://<device-ip>/api/status` or `Invoke-WebRequest http://<device-ip>/api/status | Select-Object -Expand Content | ConvertFrom-Json`

- `GET /get` (auth required)
  - Full state JSON, optimized with static buffers for low heap usage.

- `GET /health` (auth required)
  - Lightweight diagnostics: heap, flash, SPIFFS usage, history health, loop timings. Rate-limited.

- `GET /chart-data?hours=<n>` (auth required)
  - Chart series for the last `n` hours (1–168). Rate-limited and heap-guarded.
  - JSON: `{ timeHistory, tempHistory, costHistory, heaterHistory, dataPoints, rangeHours, servedHours, intervalMin, source, version }`.

- `GET /history.csv` (auth required)
  - CSV export for long-range offline analysis.

## Control

- `POST /api/mode` (auth required)
  - Change control mode (legacy and new names accepted internally). Prefer new names: `off | auto | auto-ai | advanced-ai`.
  - Example (PowerShell): `curl -X POST http://<ip>/api/mode -b "session=<token>" -F mode=auto-ai`

- `POST /setmode` (auth required)
  - Alias for mode change. Same accepted values as above.

- `POST /api/temperature` (auth required)
  - Set manual temperature (exits auto mode). JSON body: `{ "target": <float> }` or form field `target`.
  - Example (PowerShell JSON): `curl -X POST http://<ip>/api/temperature -H "Content-Type: application/json" -b "session=<token>" -d '{"target":55}'`

- `POST /api/temperature/auto` (auth required)
  - Switch back to automatic control (clears manual override).

- `POST /reset-heat-cycles` (auth required)
  - Resets daily/total heat cycle counters.

- `POST /force-history-save` (auth required)
  - Persist chart history immediately.

## Configuration & AI

- `GET /api/config` (auth required)
  - Returns configuration object (mirrors main.cpp shape). Includes timezone when set.

- `POST /api/config` (auth required)
  - Update configuration fields. Timezone/NTP updates applied immediately.

- `GET /api/ai/config` (auth required)
  - Returns AI config and metrics summary (hasEnoughData, optimizationCycles, efficiencyScore, comfortScore, costScore, confidence, heaterOnTime*, avgTempDeviation*, etc.).

- `POST /api/ai/config` (auth required)
  - Updates AI-related configuration fields.

- `POST /api/ai/optimize` (auth required)
  - Trigger AI learning cycle.

- `POST /api/ai/reset` (auth required)
  - Reset AI configuration/state to defaults.

- `POST /api/ai/learning-rate` (auth required)
  - Update AI learning rate. Body: `{ "learningRate": <float> }`.

- `GET /neural-status` (auth required)
  - Detailed neural status (large JSON). Heap-guarded; only request sparingly.

- `GET /neural-status-public` (public)
  - Minimal neural info: `{ trainingEpochs, networkConfidence }`. Designed for safe polling.

## Authentication Helpers

- `POST /login` – returns `{status, token, role}` when `Accept: application/json` is set; also sets cookie for browsers.
  - PowerShell JSON example:
    - `curl -X POST http://<ip>/login -H "Accept: application/json" -H "Content-Type: application/json" -d '{"login":"evg","password":"***"}'`
  - Parse token:
    - `$resp = Invoke-RestMethod -Uri "http://<ip>/login" -Headers @{Accept='application/json'} -Method Post -ContentType 'application/json' -Body '{"login":"evg","password":"***"}'`
    - `$token = $resp.token`
    - Use it: `curl http://<ip>/api/status -H "Authorization: Bearer $token"`

- `GET /auth-check` (public) – light probe for UI to check session without heavy data.
- `POST /logout` – clears the caller’s session and expires the cookie.

Cookie handling on Windows PowerShell:
- You can use cookies (`-b "session=<token>"`), but for headless clients prefer `Authorization: Bearer <token>`.

## Security Best Practices

- Network-level protection: keep device on trusted LAN; avoid exposing to the internet. If remote access is needed, use VPN or a reverse proxy with HTTPS and Basic/Auth in front.
- Sessions: rotate session tokens after firmware updates; use strong admin credentials; keep `vic` as limited role.
- Rate limiting: built-in server-side global limiter protects against multi-tab overload. Avoid running multiple dashboards simultaneously.
- Memory safety: heavy endpoints guard heap and may return 503/429. UIs should handle these gracefully and back off.
- CORS: most JSON responses add `Access-Control-Allow-Origin: *` for compatibility; enforce auth headers when controlling from another origin.

## Notes on Stability

- Chart endpoint is downsized and rate-limited to prevent reboots due to heap exhaustion when multiple clients request long ranges.
- `GET /api/status` is the preferred polling endpoint; avoid polling `/get`, `/neural-status`, or `/chart-data` faster than necessary.
- Daily heat cycles reset at midnight independently of heater activity, and also on the first cycle after a day change.
- If you experience repeated 503/429 responses, close extra tabs and reduce chart range until the device settles.

## Quick Reference (PowerShell examples)

- Status: `curl http://<ip>/api/status`
- Change mode (Bearer): `curl -X POST http://<ip>/api/mode -H "Authorization: Bearer <token>" -F mode=advanced-ai`
- Set manual temp: `curl -X POST http://<ip>/api/temperature -H "Content-Type: application/json" -b "session=<token>" -d '{"target":52.5}'`
- Back to auto temp: `curl -X POST http://<ip>/api/temperature/auto -b "session=<token>"`
- AI optimize: `curl -X POST http://<ip>/api/ai/optimize -b "session=<token>"`
- AI reset: `curl -X POST http://<ip>/api/ai/reset -b "session=<token>"`
- Neural minimal: `curl http://<ip>/neural-status-public`

## Remote Controller Checklist
- Use `/login` with `Accept: application/json` to obtain `token`.
- Send `Authorization: Bearer <token>` on every control/config request.
- Poll `GET /api/status` every 1–2 seconds max; back off on HTTP 429/503.
- For charts, prefer small windows and low frequency; avoid concurrent requests.
- Handle 401 by re-login; handle 429/503 with retry after a short delay.

## Samba Bridge Endpoints (if used)
- `GET /api/samba/status` (auth) – bridge health, last backup time.
- `POST /api/samba/backup` (auth) – snapshot NVRAM chart to Samba (via HTTP bridge).
- `POST /api/samba/restore-latest` (auth) – restore the latest Samba snapshot into NVRAM (guarded for empty snapshots).

## Future Hardening Ideas

- Add HMAC-signed requests or token parameter for headless clients instead of cookies.
- Expose a read-only `vic` API scope (subset of `/get`) with header-based auth only.
- Implement exponential backoff headers for clients (Retry-After) on 429/503.
- Persist server-side audit log of control actions.
