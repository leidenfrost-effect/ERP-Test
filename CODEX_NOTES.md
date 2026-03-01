# Codex Notes

Track decisions, assumptions, and follow-up items here.

## 0000-00-00 00:00
- Decision:
- Why:
- Next:

## 2026-03-01 14:56
- Initial import of current project state and baseline repository setup

## 2026-03-01 15:46
- Decision: Add protobuf route parity for product/stock endpoints in API (/pb/products*, /pb/stock*).
- Why: JSON endpoints already existed; binary clients needed equivalent coverage and consistent auth/error semantics.
- Next: Add API-level smoke checks for new protobuf product/stock endpoints in scripts/smoke-test.ps1.

## 2026-03-01 17:20
- Decision: Implement P2 security quality gates as lightweight scripts + CI workflow, keeping local runs tool-optional.
- Why: Team can run a partial gate locally without blocking, while CI enforces security scan/SBOM artifacts consistently.
- Next: Implement P3 observability/audit hardening in API with correlation/trace context and audit event schema docs.

## 2026-03-01 19:05
- Decision: Implement P3 observability and audit as cross-cutting API finalization logic.
- Why: Ensures all endpoints consistently return trace headers and all successful write operations emit audit evidence without per-route duplication.
- Next: Add persistence-backed immutable audit storage in database layer (current baseline emits structured logs only).
