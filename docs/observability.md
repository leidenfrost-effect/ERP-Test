# Observability

## Correlation and Trace Headers

The API now emits these headers on every response:

- `X-Request-Id`
- `X-Correlation-Id`
- `traceparent` (W3C Trace Context format)

Behavior:

- If request contains `X-Request-Id`, it is reused.
- If request contains `X-Correlation-Id`, it is reused (capped length).
- If request contains a valid `traceparent`, it is propagated.
- Otherwise, the API generates deterministic IDs per request.

## Access Logging

Structured access logs (`event=http_access`) include:

- `request_id`
- `correlation_id`
- `trace_id`
- `method`, `path`, `status`, `duration_ms`

## Notes

- Trace IDs and correlation IDs are safe to share in tickets/incidents.
- Do not log bearer tokens directly; audit logs use token hash tagging only.
