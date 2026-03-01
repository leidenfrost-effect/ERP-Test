---
name: hybrid-person-mvp
description: Maintain and extend the Hybrid Person MVP repository (Flutter mobile -> Crow REST API -> C++ core business -> SQLite with protobuf contracts). Use when changing architecture, contracts, persistence, API routes, tests, or build flow in this project.
---

# Hybrid Person MVP

## Follow Layer Boundaries

- Keep `core/` independent from HTTP, SQLite, and protobuf.
- Keep repository interface in `core/` and concrete repository in `infra/`.
- Keep `api/` handlers thin and delegate business rules to `core::PersonService`.
- Keep protobuf contracts in `proto/person.proto` and mappings in `infra/src/mapping`.

## Apply Changes In Order

1. Update protobuf messages in `proto/person.proto` if contract changes.
2. Update mapping functions in `infra/src/mapping/person_mapper.cpp`.
3. Update domain/service rules in `core/`.
4. Update SQLite repository/migrations in `infra/src/sqlite/`.
5. Update Crow routes in `api/src/main.cpp`.
6. Update tests in `tests/` for every behavioral change.

## Build And Test

1. Configure:
   - `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"`
2. Build:
   - `cmake --build build --config Release`
3. Test:
   - `ctest --test-dir build -C Release --output-on-failure`

## Run API

- `.\build\api\Release\person_api.exe .\build\persons.db 18080`

## Keep Planning Doc In Sync

- Use `references/agenda.md` as the working agenda for this project skill.
- Keep root-level `agenda.md` aligned with `references/agenda.md`.
