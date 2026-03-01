# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]
- Added P3 observability + audit baseline:
  - API now emits `X-Correlation-Id` and W3C `traceparent` headers.
  - Access logs now include `correlation_id` and `trace_id`.
  - Successful write operations emit structured `event=audit` logs.
  - Added audit schema: `security/audit-event.schema.json`.
  - Added docs: `docs/observability.md`, `docs/audit-logging.md`.
- Added P2 quality-gate baseline:
  - `ops/scan_all.sh`, `ops/quality_gate.sh`, `ops/gen_sbom.sh`
  - `ops/contract_test.sh`, `ops/gen_client.sh`, `ops/install_tools.md`
  - `.github/workflows/quality-gate.yml`
- Added OpenAPI/contract and CI hardening docs:
  - `docs/api/README.md`
  - `docs/quality-gates.md`
  - `docs/sbom.md`
  - `docs/ci-alternatives.md`
- Updated repository docs for security automation and ignored local artifacts (`artifacts/`, `build-check/`).
- Added protobuf endpoint parity for product and stock APIs (`/pb/products*`, `/pb/stock*`) and updated docs.
- Expanded mapping round-trip coverage to include product and stock protobuf mappings.
- Initial import of current project state and baseline repository setup
- Initial setup.
