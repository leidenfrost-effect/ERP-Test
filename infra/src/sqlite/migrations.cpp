#include "infra/sqlite/migrations.hpp"

#include <string>

#include "core/error.hpp"

namespace infra::sqlite {
namespace {

core::Error MigrationError(const std::string& message, std::string detail = "") {
  core::Error error{
    core::ErrorCode::RepositoryError,
    message,
    {}
  };
  if (!detail.empty()) {
    error.details.emplace_back(std::move(detail));
  }
  return error;
}

}  // namespace

core::Result<bool> RunMigrations(sqlite3* db) {
  if (db == nullptr) {
    return core::Result<bool>::Failure(MigrationError("SQLite database handle is null."));
  }

  const char* sql = R"sql(
    CREATE TABLE IF NOT EXISTS persons (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      first_name TEXT NOT NULL,
      last_name TEXT NOT NULL,
      email TEXT NOT NULL UNIQUE,
      age INTEGER NOT NULL CHECK(age >= 0 AND age <= 130)
    );

    CREATE UNIQUE INDEX IF NOT EXISTS idx_persons_email ON persons(email);

    CREATE TABLE IF NOT EXISTS products (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      sku TEXT NOT NULL UNIQUE,
      name TEXT NOT NULL,
      category TEXT NOT NULL,
      default_uom TEXT NOT NULL,
      product_type TEXT NOT NULL CHECK(product_type IN ('finished', 'semi', 'raw')),
      is_stock_tracked INTEGER NOT NULL CHECK(is_stock_tracked IN (0, 1)),
      safety_stock INTEGER NOT NULL CHECK(safety_stock >= 0),
      reorder_point INTEGER NOT NULL CHECK(reorder_point >= 0)
    );

    CREATE UNIQUE INDEX IF NOT EXISTS idx_products_sku ON products(sku);
    CREATE INDEX IF NOT EXISTS idx_products_name ON products(name);

    CREATE TABLE IF NOT EXISTS warehouses (
      code TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      allow_negative_stock INTEGER NOT NULL DEFAULT 0 CHECK(allow_negative_stock IN (0, 1))
    );

    INSERT OR IGNORE INTO warehouses(code, name, allow_negative_stock)
    VALUES ('MAIN', 'Main Warehouse', 0);

    CREATE TABLE IF NOT EXISTS stock_balances (
      product_id INTEGER NOT NULL,
      warehouse_code TEXT NOT NULL,
      on_hand INTEGER NOT NULL DEFAULT 0,
      reserved INTEGER NOT NULL DEFAULT 0 CHECK(reserved >= 0),
      PRIMARY KEY(product_id, warehouse_code),
      FOREIGN KEY(product_id) REFERENCES products(id) ON DELETE CASCADE,
      FOREIGN KEY(warehouse_code) REFERENCES warehouses(code) ON DELETE RESTRICT
    );

    CREATE INDEX IF NOT EXISTS idx_stock_balances_warehouse ON stock_balances(warehouse_code);

    CREATE TABLE IF NOT EXISTS stock_ledger (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      product_id INTEGER NOT NULL,
      warehouse_code TEXT NOT NULL,
      movement_type TEXT NOT NULL,
      quantity_delta INTEGER NOT NULL,
      reason TEXT NOT NULL,
      created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
      FOREIGN KEY(product_id) REFERENCES products(id) ON DELETE CASCADE,
      FOREIGN KEY(warehouse_code) REFERENCES warehouses(code) ON DELETE RESTRICT
    );

    CREATE INDEX IF NOT EXISTS idx_stock_ledger_product_warehouse
      ON stock_ledger(product_id, warehouse_code, created_at);
  )sql";

  char* error_message = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error_message);
  if (rc != SQLITE_OK) {
    const std::string detail = (error_message != nullptr) ? error_message : sqlite3_errmsg(db);
    sqlite3_free(error_message);
    return core::Result<bool>::Failure(MigrationError("Failed to run SQLite migrations.", detail));
  }

  return core::Result<bool>::Success(true);
}

}  // namespace infra::sqlite
