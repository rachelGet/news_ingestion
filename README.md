# news_ingestion — News Ingestion & Sentiment Scoring

C++/Qt6 service that drains unprocessed news articles from a private **PostgreSQL** source, runs each one through a **sentiment scorer**, and writes the scored output into **SQL Server**. The pipeline is woken up on a recurring schedule by a **Dapr cron input binding**, but it can also be triggered manually over HTTP.

This module is one of the data extractors of the [Market Profiling & OSINT Sensemaking](../README.md) project.

---

## What it does

The pipeline is three steps, executed on every tick:

1. **Fetch** unprocessed news from PostgreSQL (`DataExtractor::fetchUnprocessedNews`).
2. **Score** each article (`SentimentScorer::score`) producing a `NewsScored` record with `url_hash`, `snmt_score`, `publish_date`, and `tags`.
3. **Store** the batch into SQL Server via `DataStore::saveScoredBatch` (table auto-created on startup by `ensureTable`).

The service exposes an embedded HTTP server on port `3003`. The `news-tranformer` Dapr sidecar is configured with a cron input binding that POSTs to `/scheduler-conn` every 2 seconds (see [components/conn_scheduler.yaml](components/conn_scheduler.yaml)). Each tick processes up to 50 items; the manual `/fetch-all` endpoint processes up to 100.

---

## Architecture

```
   ┌────────────────────┐    POST /scheduler-conn    ┌─────────────────────┐
   │ Dapr cron binding  │ ─────────────────────────► │ news_ingestion      │
   │  (@every 2s)       │                            │  port 3003          │
   └────────────────────┘                            │                     │
                                                     │  DataExtractor      │
   ┌────────────────────┐  SELECT unprocessed        │      │              │
   │ PostgreSQL (priv.) │ ◄──────────────────────────┤      ▼              │
   │  raw news          │                            │  SentimentScorer    │
   └────────────────────┘                            │      │              │
                                                     │      ▼              │
                                                     │  DataStore          │
                                                     └──────────┬──────────┘
                                                                │ INSERT
                                                                ▼
                                                     ┌─────────────────────┐
                                                     │ SQL Server          │
                                                     │  newsdb / scored    │
                                                     └─────────────────────┘
```

The Dapr sidecar (`news-tranformer`, HTTP `:3503`) only delivers cron triggers — the pipeline itself talks to PostgreSQL and SQL Server directly using the connection strings in environment variables.

---

## Repository layout

| Path | Purpose |
| :--- | :--- |
| [Main.cpp](Main.cpp) | Boots Qt, sets up the embedded HTTP server, wires the `runPipeline` function. |
| [DataExtraction.cpp](DataExtraction.cpp) | PostgreSQL reader — `fetchUnprocessedNews(limit)`. |
| [SentimentScorer.cpp](SentimentScorer.cpp) | Scores a `NewsRaw` and returns a `NewsScored`. |
| [DataStore.cpp](DataStore.cpp) | SQL Server writer — `ensureTable`, `saveScoredBatch`, `isConnected`. |
| [NewsTypes.h](NewsTypes.h) | `NewsRaw` and `NewsScored` POD types. |
| [components/conn_scheduler.yaml](components/conn_scheduler.yaml) | Dapr cron input binding (`@every 2s` → `POST /scheduler-conn`). |
| [style.qss](style.qss) | Qt stylesheet. |
| [CMakeLists.txt](CMakeLists.txt) | Build target. |

---

## Requirements

| Requirement | Version / Notes |
| :--- | :--- |
| **C++** | C++17 (gcc 11.4+) |
| **CMake** | 3.16+ |
| **Qt** | Qt6 — `Core Network Sql` |
| **PostgreSQL** | Reachable source DB containing the raw news table. |
| **SQL Server** | 2022/2025 (provided by `sql1` in the parent compose). |
| **Dapr** | 1.15.1 (`news-tranformer` sidecar on HTTP `:3503`). |

---

## Environment variables

| Variable | Required | Default | Purpose |
| :--- | :--- | :--- | :--- |
| `CONN_STR_POSTGRES` | yes | — | Full libpq connection string for the PostgreSQL **source**. |
| `PASSWORD_SQL` | yes | — | SA password for the SQL Server **destination**. |
| `SQL_SERVER` | no | `localhost,1433` | Host,port of SQL Server. |
| `SQL_DATABASE` | no | `newsdb` | Destination database name. |
| `SQL_USER` | no | `sa` | Destination user. |

The service refuses to start if either `CONN_STR_POSTGRES` or `PASSWORD_SQL` is missing.

---

## Build

```bash
# from news_ingestion/
cmake -S . -B build
cmake --build build -j
```

Output: `build/news_ingestion`.

---

## Run

1. **Export the required env vars:**
   ```bash
   export CONN_STR_POSTGRES="host=... port=5432 user=... password=... dbname=..."
   export PASSWORD_SQL="..."
   ```
2. **Bring up SQL Server + the `news-tranformer` sidecar** from the project root:
   ```bash
   docker compose up -d sql1 data-news-component
   ```
3. **Launch the service:**
   ```bash
   ./build/news_ingestion
   ```
   The HTTP server binds to `0.0.0.0:3003`. The Dapr sidecar will start POSTing to `/scheduler-conn` every 2 seconds, and the pipeline will begin draining unprocessed news.

### HTTP endpoints

| Method | Path | Purpose |
| :--- | :--- | :--- |
| `POST` | `/scheduler-conn` | Cron tick (Dapr input binding). Processes up to **50** items. |
| `POST` | `/fetch-all` | Manual run. Processes up to **100** items. |
| `GET` | `/health` | Liveness — reports PostgreSQL and SQL Server connection state. |

Each successful tick returns a JSON body with:

```json
{
  "fetched": 12,
  "scored": 12,
  "stored": 12,
  "fetched_at": "2026-04-09T12:34:56Z",
  "items": [ { "url_hash": "...", "snmt_score": 0.42, "publish_date": "...", "tags": "..." } ]
}
```

Manual trigger from the host:
```bash
curl -X POST http://localhost:3003/fetch-all
curl http://localhost:3003/health
```

Runtime logs are written to [news_transformer.log](news_transformer.log) next to the binary.

---

## Troubleshooting

| Symptom | Likely cause |
| :--- | :--- |
| `ERROR: CONN_STR_POSTGRES not set` / `PASSWORD_SQL not set` | Export the variables before launching. |
| `WARNING: SQL Server not connected` | `sql1` is not yet healthy or `PASSWORD_SQL` is wrong — the service will keep retrying on each tick. |
| `/scheduler-conn` never fires | `data-news-component` Dapr sidecar is not running — check `docker compose logs data-news-component`. |
| `fetched=0` on every tick | PostgreSQL has no unprocessed rows, or the source query in `DataExtraction.cpp` is filtering them out. |
| `Failed to listen on port 3003` | Another process already holds the port — `ss -ltnp | grep 3003`. |
