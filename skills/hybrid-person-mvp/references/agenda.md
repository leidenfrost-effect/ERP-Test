# Agenda - Hybrid C++/Flutter MVP

## Goal

Deliver a working MVP with clean layer boundaries:

- Flutter mobile client
- C++ Crow REST API
- C++ core business rules
- SQLite persistence via sqlite3 C API
- Protobuf contract + JSON transport

## Done (MVP Scope)

1. Project skeleton and CMake wiring.
2. `proto/person.proto` with request/response and `ApiError`.
3. Core layer:
   - `Person` entity
   - validation rules
   - `IPersonRepository` interface
   - `PersonService` business layer
4. Infra layer:
   - SQLite repository implementation
   - migration bootstrap
   - protobuf mapping functions
5. API layer:
   - Crow server
   - `/health` and CRUD person endpoints
   - protobuf<->JSON conversion
   - mapped API errors
6. Tests:
   - core validation/service logic
   - sqlite CRUD against temp DB
   - mapping round-trip
7. Mobile:
   - basic Flutter CRUD consumer for the API
8. Documentation:
   - setup/build/run guide
   - curl examples

## Completed Enhancements

1. Pagination/filtering added for `GET /persons` and `GET /pb/persons`:
   - query params: `page`, `pageSize`, `q`
   - response metadata: `page`, `pageSize`, `total`, `query`
2. Binary protobuf endpoint variants added under `/pb/*`.
3. SQLite hardening applied:
   - `WAL` mode and `synchronous=NORMAL`
   - `busy_timeout=5000`
   - `foreign_keys=ON`
   - explicit write transactions (`BEGIN IMMEDIATE` / `COMMIT` / rollback on failure)
   - thread-safe access via repository mutex
4. CI pipeline added with GitHub Actions matrix (`windows-latest`, `ubuntu-latest`).
5. Authentication and structured logging added:
   - optional bearer auth via `PERSON_API_TOKEN`
   - structured JSON access log lines (`method`, `path`, `status`, `duration_ms`, etc.)
6. Operational runtime extensions added:
   - readiness endpoints (`/ready`, `/pb/ready`)
   - Prometheus metrics endpoint (`/metrics`)
   - response/request correlation (`X-Request-Id`)
   - secure default response headers
   - env-driven runtime config (`PERSON_API_DB_PATH`, `PERSON_API_PORT`, `PERSON_API_CONCURRENCY`)
7. Company tooling package added:
   - `CMakePresets.json`
   - `.editorconfig`, `.clang-format`, `.clang-tidy`
   - automation scripts under `scripts/`
   - governance automation (`codeql`, `dependabot`, `pre-commit`)
   - operations docs under `docs/`
8. Product and stock management baseline implemented:
   - protobuf contract expanded with `Product*` and `Stock*` messages
   - core services added: `ProductService`, `InventoryService`
   - SQLite persistence added: `products`, `warehouses`, `stock_balances`, `stock_ledger`
   - JSON API endpoints added: `/products*`, `/stock`, `/stock/movements`
   - new tests added for core + sqlite product/inventory flows

## Expansion Scope (Project + Manufacturing + Inventory)

1. Project planning module
2. Project budget module
3. Issued order management (`sales` and/or `purchase`)
4. Product creation and catalog management
5. BOM creation and parts list management
6. Warehouse and stock management

## Detailed Project Plan (10 Weeks)

| Phase | Weeks | Main Outputs | Exit Criteria |
| --- | --- | --- | --- |
| Phase 0: Design | 1-2 | final process map, proto drafts, DB schema draft | architecture review approved |
| Phase 1: Product + BOM | 3-4 | product master, BOM revisioning, parts list APIs | product/BOM CRUD + tests passing |
| Phase 2: Orders + Budget | 5-6 | order lifecycle, budget planning, budget variance | order/budget workflows validated |
| Phase 3: Warehouse + Stock | 7-8 | warehouse/bin model, stock ledger, reservations | stock movements reconcile correctly |
| Phase 4: Hardening + UAT | 9-10 | integration tests, dashboards, runbook updates | UAT sign-off and release checklist done |

## Module Detail

### 1) Project Plan Module

- Entities: `project`, `project_phase`, `project_task`, `project_milestone`.
- Minimum project fields:
  - `project_code`, `name`, `customer`, `start_date`, `target_end_date`, `status`
  - `priority`, `owner_user_id`, `description`
- Status flow: `draft -> approved -> in_progress -> blocked -> completed -> archived`.
- Required capabilities:
  - define milestones and dependencies
  - assign tasks to owners
  - progress tracking (`planned%`, `actual%`)
  - risk/issue register per project

### 2) Budget Module

- Entities: `project_budget`, `project_budget_line`, `cost_center`.
- Budget line categories:
  - `material`, `labor`, `subcontract`, `logistics`, `overhead`, `contingency`
- Budget workflow:
  - `draft -> submitted -> approved -> locked`
  - revision support with immutable history (`v1`, `v2`, ...)
- Actual cost sources:
  - issued purchase orders
  - stock consumption from warehouse
  - manual adjustment entries (approval-required)
- KPIs:
  - `planned_cost`
  - `actual_cost`
  - `variance_amount = planned_cost - actual_cost`
  - `variance_percent = variance_amount / planned_cost`

### 3) Issued Orders Module

- Entities: `order_header`, `order_line`, `order_allocation`.
- Order types: `sales_order` and `purchase_order`.
- Status flow: `draft -> confirmed -> allocated -> shipped/received -> closed` (+ `cancelled`).
- Required capabilities:
  - create order from project context
  - reserve stock at confirmation
  - partial shipment/receipt support
  - link order lines to budget lines for cost tracking

### 4) Product Creation Module

- Entities: `product`, `product_variant`, `uom`, `product_revision`.
- Minimum product fields:
  - `sku`, `name`, `category`, `default_uom`, `product_type` (`finished`, `semi`, `raw`)
  - `is_stock_tracked`, `safety_stock`, `reorder_point`
- Validation rules:
  - unique `sku`
  - positive conversion for UOM factors
  - `raw` products cannot own BOM headers

### 5) BOM + Parts List Module

- Entities: `bom_header`, `bom_line`, `part_master`.
- BOM structure:
  - header: `product_id`, `revision`, `effective_from`, `effective_to`, `is_active`
  - lines: `part_id`, `qty_per`, `uom`, `scrap_percent`, `lead_time_days`
- BOM publishing workflow:
  - `draft -> review -> released -> obsolete`
- Required checks:
  - no circular references
  - no duplicate component lines in same revision
  - all components must be active products/parts
- Outputs:
  - exploded BOM endpoint
  - flattened parts list export (`csv/json`)
  - cost roll-up from latest standard costs

### 6) Warehouse and Stock Management Module

- Entities: `warehouse`, `bin_location`, `stock_balance`, `stock_ledger`, `stock_count`.
- Core stock movements:
  - `receipt`
  - `issue`
  - `transfer`
  - `adjustment`
  - `reservation` and `reservation_release`
- Required controls:
  - movement-level audit trail (`who`, `when`, `why`)
  - negative stock policy per warehouse (`allow`/`block`)
  - cycle count workflow with approval
  - reorder alert generation by `reorder_point` and `safety_stock`

## Initial Budget Estimate

Assumption: blended team rate = `400 USD/day` (for planning only).

| Workstream | Effort (person-days) | Estimated Cost (USD) |
| --- | --- | --- |
| Product + BOM + Parts List | 20 | 8,000 |
| Orders + Budget | 18 | 7,200 |
| Warehouse + Stock | 18 | 7,200 |
| Mobile/UI updates | 10 | 4,000 |
| QA + UAT + automation | 8 | 3,200 |
| PM + release + training | 6 | 2,400 |
| **Subtotal** | **80** | **32,000** |
| **Contingency (15%)** | - | **4,800** |
| **Total Plan Budget** | - | **36,800** |

## Priority Delivery Backlog

1. Extend protobuf contracts for project/budget/order/product/bom/stock aggregates.
2. Add SQLite migrations for all new entities and indexes.
3. Implement core services (`ProjectService`, `BudgetService`, `OrderService`, `InventoryService`).
4. Implement infra repositories and mapping layer updates.
5. Add Crow routes for CRUD + lifecycle actions.
6. Add unit and integration tests for every workflow.
7. Update Flutter app screens for project, product, BOM, orders, and warehouse operations.
8. Publish runbook additions and go-live checklist.

## Immediate Next Steps

1. Lock glossary and process decisions (sales vs purchase flow priority, reservation rules).
2. Finalize `proto` message design and migration plan in one review session.
3. Start Phase 1 implementation: product master, BOM revisioning, and parts list endpoints.
