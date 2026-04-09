-include ../.env

export 
CONN_STR_POSTGRES = postgresql://$(USER_POSTGRES):$(PASS_POGRES)@$(IP_DIRC_POSTGRES):$(PORT_POSTGRES)/$(DB_NAME)?sslmode=disable

BUILD_DIR = build
BIN       = $(BUILD_DIR)/bin/NewsTransformer

# ── Build ──

.PHONY: all build clean run fetch-all health

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

release:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

# ── Run ──

# Required env vars:
#   CONN_STR_POSTGRES  - postgres://user:pass@host:5432/db  (source: read news)
#   PASSWORD_SQL       - SQL Server password                (dest: write scored news)
# Optional env vars:
#   SQL_SERVER         - default: localhost,1433
#   SQL_DATABASE       - default: newsdb
#   SQL_USER           - default: sa

run: build
	CONN_STR_POSTGRES=${CONN_STR_POSTGRES} \
	PASSWORD_SQL=${PASSWORD_SQL} \
	SQL_SERVER=${SQL_SERVER:-localhost,1433} \
	SQL_DATABASE=${SQL_DATABASE:-newsdb} \
	SQL_USER=$${SQL_USER:-sa} \
	./$(BIN)

run-dapr: build
	dapr run \
		--app-id news-transformer \
		--app-port 3003 \
		--resources-path ./components \
		-- ./$(BIN)

# ── Manual triggers (app must be running) ──

fetch-all:
	curl -s -X POST http://localhost:3003/fetch-all | python3 -m json.tool

health:
	curl -s http://localhost:3003/health | python3 -m json.tool

# ── Clean ──

clean:
	rm -rf $(BUILD_DIR)
