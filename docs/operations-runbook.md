# Operations Runbook

## 1. Startup

### Windows

```powershell
./scripts/bootstrap.ps1 -ClearProxy
./scripts/build.ps1
./scripts/run-api.ps1 -DbPath ".\\build\\persons.db" -Port 18080 -Concurrency 4 -Token "change-me"
```

### Linux/macOS

```bash
cmake --preset linux-release
cmake --build --preset build-linux-release
PERSON_API_TOKEN=change-me ./build/api/person_api ./build/persons.db 18080 4
# strict production mode example
PERSON_API_ENV=production \
PERSON_API_TOKEN=change-me \
PERSON_API_DEFAULT_PERMISSIONS="auth.verify|persons.read|persons.write|products.read|products.write|stock.read|stock.write|metrics.read" \
./build/api/person_api ./build/persons.db 18080 4
```

## 2. Health and Readiness

- Liveness: `GET /health`
- Readiness: `GET /ready`
- Auth mode: `GET /auth/status`
- Token verification: `GET /auth/verify`

Use readiness as the deployment gate.

## 3. Metrics

- Endpoint: `GET /metrics`
- Response: Prometheus exposition format
- Include bearer auth header when token mode is enabled

## 4. Authentication Rotation

Use `PERSON_API_TOKENS` for safe token rotation:

1. Deploy with old + new token together.
2. Migrate clients to new token.
3. Remove old token from env and redeploy.

## 5. Incident Checklist

1. Check `/health`
2. Check `/ready`
3. Check `/metrics`
4. Inspect structured logs (`event=http_access`)
5. Validate DB file availability and permissions
6. Restart process with same env config

## 6. Backup Notes

The API stores data in SQLite file path configured by:

- CLI arg `db_path`
- or env `PERSON_API_DB_PATH`

Back up this file periodically and before migrations/releases.
