# Company Tooling Guide

This project now includes a production-oriented tooling stack for engineering teams.

## Runtime Operations

- Readiness endpoint: `GET /ready` and `GET /pb/ready`
- Health endpoint: `GET /health` and `GET /pb/health`
- Metrics endpoint: `GET /metrics` (Prometheus format)
- Structured access logs (JSON lines)
- Request correlation header: `X-Request-Id`
- Security headers:
  - `X-Content-Type-Options: nosniff`
  - `X-Frame-Options: DENY`
  - `Referrer-Policy: no-referrer`
  - `Cache-Control: no-store`

## Authentication

Runtime supports:

- `PERSON_API_TOKEN` for a single token
- `PERSON_API_TOKENS` for comma-separated multi-token mode
- `PERSON_API_ENV=production` to enforce strict startup checks
- `PERSON_API_TOKEN_PERMISSIONS` / `PERSON_API_DEFAULT_PERMISSIONS` for RBAC

Protected routes:

- `/persons*`
- `/pb/persons*`
- `/products*`
- `/pb/products*`
- `/stock*`
- `/pb/stock*`
- `/metrics`
- `/auth/verify`
- `/pb/auth/verify`

## Build/Test/Lint Automation

PowerShell scripts under `scripts/`:

- `bootstrap.ps1`: configure CMake
- `build.ps1`: compile project
- `test.ps1`: run CTest
- `run-api.ps1`: run API with runtime parameters
- `lint.ps1`: format/lint checks (`clang-format`, optional `clang-tidy`)
- `smoke-test.ps1`: endpoint smoke checks

## CI/CD and Governance

- CI matrix build and test:
  - `.github/workflows/ci.yml`
- Code scanning:
  - `.github/workflows/codeql.yml`
- Automated dependency updates:
  - `.github/dependabot.yml`
- Local pre-commit checks:
  - `.pre-commit-config.yaml`

## Coding Standards

- `.editorconfig`
- `.clang-format`
- `.clang-tidy`
- `CMakePresets.json` for reproducible configure/build/test flows

## Recommended Team Workflow

1. Configure: `./scripts/bootstrap.ps1 -ClearProxy`
2. Build: `./scripts/build.ps1`
3. Test: `./scripts/test.ps1`
4. Lint check: `./scripts/lint.ps1 -CheckOnly`
5. Run API: `./scripts/run-api.ps1 -Token "change-me"`
6. Smoke: `./scripts/smoke-test.ps1 -Token "change-me"`
