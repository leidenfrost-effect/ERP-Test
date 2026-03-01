# Hybrid MVP Repo (Flutter + C++ Crow + Core + SQLite + Protobuf)

This repository implements the requested hybrid architecture:

- Flutter mobile client (`/mobile`)
- C++ REST API with Crow (`/api`)
- C++ core business layer (`/core`)
- SQLite persistence via sqlite3 C API (`/infra`)
- Protobuf contracts with CMake code generation (`/proto`)
- Unit/integration tests (`/tests`)

## Folder Structure

```text
/
  api/
  core/
  docs/
  infra/
  mobile/
  proto/
  scripts/
  tests/
  README.md
  agenda.md
  skill.md
```

## Tech Stack

- C++17 + CMake
- Crow (header-only REST framework)
- sqlite3 C API
- Protobuf (`person.proto` + protoc codegen)
- Flutter + Dart

## Build Dependencies

- CMake >= 3.20
- C++17 compiler (MSVC, Clang, GCC)
- Protobuf compiler + library
- SQLite3
- Crow

### Option A: vcpkg

Install dependencies with vcpkg:

```bash
vcpkg install crow protobuf sqlite3
```

Configure/build:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<VCPKG_ROOT>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### Option B: FetchContent for Crow

Crow is already configured to fallback to `FetchContent` if a CMake package is not found.
You still need protobuf + sqlite3 available on your system.

Example:

```bash
cmake -S . -B build
cmake --build build
```

## Company Tooling

- Formatting and lint:
  - `.editorconfig`
  - `.clang-format`
  - `.clang-tidy`
- Reproducible configure/build/test:
  - `CMakePresets.json`
- Dev scripts:
  - `scripts/bootstrap.ps1`
  - `scripts/build.ps1`
  - `scripts/test.ps1`
  - `scripts/lint.ps1`
  - `scripts/run-api.ps1`
  - `scripts/smoke-test.ps1`
- Governance and security:
  - `.github/workflows/ci.yml`
  - `.github/workflows/codeql.yml`
  - `.github/dependabot.yml`
  - `.pre-commit-config.yaml`
- Operational docs:
  - `docs/company-tooling.md`
  - `docs/operations-runbook.md`

## Run API

Executable:

- Windows (typical): `build\\api\\Release\\person_api.exe`
- Linux/macOS (typical): `build/api/person_api`

Usage:

```bash
person_api [db_path] [port] [concurrency]
```

Examples:

```bash
# default: persons.db and port 18080
person_api

# custom db and port
person_api data/persons.db 8080

# custom db, port and concurrency
person_api data/persons.db 8080 4
```

Optional auth token (if set, protected routes require bearer auth):

```bash
# Linux/macOS
export PERSON_API_TOKEN="change-me"
export PERSON_API_TOKENS="mobile-token,ops-token" # optional multi-token mode
export PERSON_API_ENV="development"               # set production for strict mode

# Optional RBAC mapping (required in production when token auth is enabled)
export PERSON_API_TOKEN_PERMISSIONS="mobile-token=auth.verify|persons.read|products.read|stock.read,ops-token=*"
# or fallback permission set for all authenticated tokens:
# export PERSON_API_DEFAULT_PERMISSIONS="auth.verify|persons.read|products.read|stock.read"
export PERSON_API_DB_PATH="./build/persons.db"
export PERSON_API_PORT="18080"
export PERSON_API_CONCURRENCY="4"

# Windows PowerShell
$env:PERSON_API_TOKEN="change-me"
$env:PERSON_API_TOKENS="mobile-token,ops-token"
$env:PERSON_API_ENV="development"
$env:PERSON_API_TOKEN_PERMISSIONS="mobile-token=auth.verify|persons.read|products.read|stock.read,ops-token=*"
$env:PERSON_API_DB_PATH=".\build\persons.db"
$env:PERSON_API_PORT="18080"
$env:PERSON_API_CONCURRENCY="4"
```

## REST Endpoints

- `GET /health`
- `GET /ready`
- `GET /auth/status`
- `GET /auth/verify`
- `GET /metrics` (requires token when auth is enabled)
- `GET /persons?page=1&pageSize=20&q=ada`
- `GET /persons/{id}`
- `POST /persons`
- `PUT /persons/{id}`
- `DELETE /persons/{id}`
- `GET /products?page=1&pageSize=20&q=fg`
- `GET /products/{id}`
- `POST /products`
- `PUT /products/{id}`
- `DELETE /products/{id}`
- `GET /stock?productId=1&warehouse=MAIN`
- `POST /stock/movements`

Binary protobuf variants (content type: `application/x-protobuf`):

- `GET /pb/health`
- `GET /pb/ready`
- `GET /pb/auth/status`
- `GET /pb/auth/verify`
- `GET /pb/persons?page=1&pageSize=20&q=ada`
- `GET /pb/persons/{id}`
- `POST /pb/persons`
- `PUT /pb/persons/{id}`
- `DELETE /pb/persons/{id}`
- `GET /pb/products?page=1&pageSize=20&q=fg`
- `GET /pb/products/{id}`
- `POST /pb/products`
- `PUT /pb/products/{id}`
- `DELETE /pb/products/{id}`
- `GET /pb/stock?productId=1&warehouse=MAIN`
- `POST /pb/stock/movements`

## Protobuf JSON Contract Notes

HTTP transport in MVP is JSON generated from Protobuf messages.
Field names become lowerCamelCase in JSON output by default.

### Sample curl Requests

```bash
curl http://127.0.0.1:18080/health
```

```bash
curl http://127.0.0.1:18080/ready
```

```bash
curl http://127.0.0.1:18080/persons
```

```bash
curl "http://127.0.0.1:18080/persons?page=1&pageSize=10&q=ada"
```

```bash
curl http://127.0.0.1:18080/persons/1
```

```bash
curl -X POST http://127.0.0.1:18080/persons \
  -H "Content-Type: application/json" \
  -d "{\"person\":{\"firstName\":\"Ada\",\"lastName\":\"Lovelace\",\"email\":\"ada@example.com\",\"age\":36}}"
```

```bash
curl -X PUT http://127.0.0.1:18080/persons/1 \
  -H "Content-Type: application/json" \
  -d "{\"person\":{\"firstName\":\"Ada\",\"lastName\":\"Byron\",\"email\":\"ada.byron@example.com\",\"age\":37}}"
```

```bash
curl -X DELETE http://127.0.0.1:18080/persons/1
```

```bash
curl -X POST http://127.0.0.1:18080/products \
  -H "Content-Type: application/json" \
  -d "{\"product\":{\"sku\":\"FG-001\",\"name\":\"Widget\",\"category\":\"Finished\",\"defaultUom\":\"EA\",\"productType\":\"PRODUCT_TYPE_FINISHED\",\"isStockTracked\":true,\"safetyStock\":2,\"reorderPoint\":5}}"
```

```bash
curl "http://127.0.0.1:18080/products?page=1&pageSize=10&q=FG"
```

```bash
curl -X POST http://127.0.0.1:18080/stock/movements \
  -H "Content-Type: application/json" \
  -d "{\"movement\":{\"productId\":1,\"warehouseCode\":\"MAIN\",\"movementType\":\"STOCK_MOVEMENT_TYPE_RECEIPT\",\"quantity\":\"10\",\"reason\":\"initial_receipt\"}}"
```

```bash
curl "http://127.0.0.1:18080/stock?productId=1&warehouse=MAIN"
```

```bash
curl -X GET "http://127.0.0.1:18080/persons?page=1&pageSize=10" \
  -H "Authorization: Bearer change-me"
```

```bash
curl -X GET http://127.0.0.1:18080/metrics \
  -H "Authorization: Bearer change-me"
```

### Sample Binary Protobuf Requests

Fetch protobuf list response and save bytes:

```bash
curl -X GET "http://127.0.0.1:18080/pb/persons?page=1&pageSize=10" \
  -H "Authorization: Bearer change-me" \
  -H "Accept: application/x-protobuf" \
  --output persons.pb
```

```bash
curl -X GET "http://127.0.0.1:18080/pb/products?page=1&pageSize=10" \
  -H "Authorization: Bearer change-me" \
  -H "Accept: application/x-protobuf" \
  --output products.pb
```

```bash
curl -X GET "http://127.0.0.1:18080/pb/stock?productId=1&warehouse=MAIN" \
  -H "Authorization: Bearer change-me" \
  -H "Accept: application/x-protobuf" \
  --output stock.pb
```

Health check in protobuf:

```bash
curl -X GET http://127.0.0.1:18080/pb/health \
  -H "Accept: application/x-protobuf" \
  --output health.pb
```

```bash
curl -X GET http://127.0.0.1:18080/pb/ready \
  -H "Accept: application/x-protobuf" \
  --output ready.pb
```

## Logging

API logs are emitted as structured JSON lines (one line per request), including:

- `ts_ms`
- `level`
- `event`
- `method`
- `path`
- `status`
- `duration_ms`
- `request_id`

Each response includes `X-Request-Id` for cross-service tracing.

## Tests

Configured with CTest:

- core validation test
- sqlite repository CRUD test (temp DB)
- mapping round-trip test
- core product/inventory service test
- sqlite product/inventory repository test

Run:

```bash
ctest --test-dir build -C Release --output-on-failure
```

Lint/format checks:

```powershell
./scripts/lint.ps1 -CheckOnly
```

## CI

GitHub Actions workflow: `.github/workflows/ci.yml`

- OS matrix: `windows-latest`, `ubuntu-latest`
- Steps: lint, configure, build, test

Additional automation:

- Code scanning: `.github/workflows/codeql.yml`
- Dependency updates: `.github/dependabot.yml`
- Security quality gate + SBOM: `.github/workflows/quality-gate.yml`
- Local wrappers:
  - `ops/scan_all.sh`
  - `ops/quality_gate.sh`
  - `ops/gen_sbom.sh`
  - `ops/contract_test.sh`
  - `ops/gen_client.sh`

## OpenAPI Contract

- Source: `openapi/openapi.yaml`
- Includes auth status/verify, persons, products, stock, metrics endpoints
- Workflow docs: `docs/api/README.md`

## Flutter Mobile

Client app is under `/mobile`.

```bash
cd mobile
flutter pub get
flutter run
```

Default API base URL in app: `http://10.0.2.2:18080` (Android emulator).
Update in `mobile/lib/main.dart` for other targets.
