# Audit Logging

## Scope

The API emits a structured `event=audit` log entry for successful write operations:

- `POST`
- `PUT`
- `PATCH`
- `DELETE`

Audit entries are generated automatically in the API response finalization path.

## Event Shape

Schema file:

- `security/audit-event.schema.json`

Current fields:

- `ts_ms`
- `event` (`audit`)
- `request_id`
- `correlation_id`
- `trace_id`
- `action`
- `resource_type`
- `resource_id`
- `actor` (`anonymous` or hashed token tag)
- `status`

## Data Minimization

- Bearer tokens are never logged in raw form.
- Actor identity is represented as token hash tag when authenticated.
- Keep retention and access control policies externalized per environment.
