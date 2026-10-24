# Enro. Ontime everytime.
# Run `make help` for the list. Fake mode needs zero API keys.

SHELL := /bin/bash
BUILD ?= build
PORT ?= 8080
BASE ?= http://localhost:$(PORT)
E2E_PORT ?= 18080
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

# Every provider forced to its fake. Used by dev-fake, e2e, and test so a stray .env can't make real calls.
FAKE_ENV := ENRO_ENV_FILE=/dev/null VOICE_PROVIDER=sim LLM_PROVIDER=fake REFLEX_PROVIDER=rules \
            CALENDAR_SOURCE=seed EMAIL_SOURCE=seed

.PHONY: help build backend frontend frontend-install dev dev-fake tunnel test contracts smoke e2e sim prerender todos clean

help: ## List targets
	@grep -E '^[a-z-]+:.*## ' $(MAKEFILE_LIST) | awk -F':.*## ' '{printf "  %-16s %s\n", $$1, $$2}'

build: ## Configure and build the backend
	cmake -S backend -B $(BUILD) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build $(BUILD) -j $(JOBS)

backend: build ## Run the backend with your .env
	./$(BUILD)/enro

frontend-install:
	@[ -d frontend/node_modules ] || npm --prefix frontend install

frontend: frontend-install ## Run the Vite dev server on :5173 (mocks unless VITE_USE_MOCKS=false)
	npm --prefix frontend run dev

dev: build frontend-install ## Backend (your .env) + frontend against the live backend
	@trap 'kill 0' EXIT INT TERM; \
	./$(BUILD)/enro & \
	VITE_USE_MOCKS=false npm --prefix frontend run dev & \
	wait

dev-fake: build frontend-install ## Backend in fake mode + frontend against it. The demo flow, no keys
	@trap 'kill 0' EXIT INT TERM; \
	$(FAKE_ENV) ./$(BUILD)/enro & \
	VITE_USE_MOCKS=false npm --prefix frontend run dev & \
	wait

tunnel: ## ngrok to the backend port. Copy the https URL into PUBLIC_BASE_URL in .env
	ngrok http $(PORT)

test: build ## Backend doctest suite
	$(FAKE_ENV) ctest --test-dir $(BUILD) --output-on-failure

contracts: ## Validate every contracts/examples file against its schema
	python3 scripts/check_contracts.py

smoke: ## Hit every section 11 route on a running backend (override with BASE=...)
	python3 scripts/smoke_api.py $(BASE)

e2e: build ## Headless golden path in fake mode: boot, smoke, wake ladder, email drop, trip, load sim
	PORT=$(E2E_PORT) BUILD=$(BUILD) scripts/e2e.sh

sim: ## 100,000-user load simulation on a running backend
	@curl -sf -X POST $(BASE)/api/sim/load -H 'Content-Type: application/json' \
	  -d '{"users": 100000, "threads": 8}' | python3 -m json.tool

prerender: ## Cache scripted TTS lines into backend/audio_cache (needs OPENAI_API_KEY, no-op without)
	scripts/prerender.sh

todos: ## Open STUB/TODO work, grouped by task
	@grep -rnoE 'TODO\((T[0-9]+|S[0-9]+)\).*' backend/src backend/tests frontend/src scripts 2>/dev/null \
	  | sed -E 's/^([^:]+:[0-9]+):TODO\(([A-Z][0-9]+)\): ?(.*)/\2\t\1\t\3/' \
	  | sort -t$$'\t' -k1,1V -k2,2 \
	  | awk -F'\t' '$$1 != t { if (t) print ""; t = $$1; n = 0; printf "%s\n", t } { printf "  %-48s %s\n", $$2, $$3 }'
	@printf '\n'; grep -rhoE 'TODO\((T[0-9]+|S[0-9]+)\)' backend/src backend/tests frontend/src scripts 2>/dev/null \
	  | sort | uniq -c | sort -k2,2V | awk '{printf "%s %s  ", $$2, $$1} END {print ""}' | sed 's/TODO(//g; s/)//g'

clean: ## Remove the build dir
	rm -rf $(BUILD)
