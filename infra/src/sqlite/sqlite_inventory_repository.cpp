#include "infra/sqlite/sqlite_inventory_repository.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "core/error.hpp"
#include "infra/sqlite/migrations.hpp"

namespace infra::sqlite {
namespace {

class Statement {
 public:
  explicit Statement(sqlite3_stmt* statement) : statement_(statement) {}

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  ~Statement() {
    if (statement_ != nullptr) {
      sqlite3_finalize(statement_);
    }
  }

  sqlite3_stmt* get() const { return statement_; }

 private:
  sqlite3_stmt* statement_{nullptr};
};

core::Error SqliteError(sqlite3* db, const std::string& context) {
  return core::Error{
    core::ErrorCode::RepositoryError,
    context,
    { db == nullptr ? "SQLite database handle is null." : sqlite3_errmsg(db) }
  };
}

bool ConfigureDatabase(sqlite3* db, std::string* error) {
  const char* sql = R"sql(
    PRAGMA foreign_keys = ON;
    PRAGMA journal_mode = WAL;
    PRAGMA synchronous = NORMAL;
  )sql";

  char* raw_error = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &raw_error);
  if (rc == SQLITE_OK) {
    return true;
  }

  if (error != nullptr) {
    *error = raw_error == nullptr ? sqlite3_errmsg(db) : raw_error;
  }
  sqlite3_free(raw_error);
  return false;
}

class WriteTransaction {
 public:
  explicit WriteTransaction(sqlite3* db) : db_(db) {}

  core::Result<bool> Begin(const std::string& context) {
    if (active_) {
      return core::Result<bool>::Success(true);
    }

    char* raw_error = nullptr;
    const int rc = sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &raw_error);
    if (rc != SQLITE_OK) {
      const std::string detail = raw_error == nullptr ? sqlite3_errmsg(db_) : raw_error;
      sqlite3_free(raw_error);
      return core::Result<bool>::Failure(core::Error{
        core::ErrorCode::RepositoryError,
        context,
        { detail }
      });
    }

    active_ = true;
    return core::Result<bool>::Success(true);
  }

  core::Result<bool> Commit(const std::string& context) {
    if (!active_) {
      return core::Result<bool>::Success(true);
    }

    char* raw_error = nullptr;
    const int rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &raw_error);
    if (rc != SQLITE_OK) {
      const std::string detail = raw_error == nullptr ? sqlite3_errmsg(db_) : raw_error;
      sqlite3_free(raw_error);
      return core::Result<bool>::Failure(core::Error{
        core::ErrorCode::RepositoryError,
        context,
        { detail }
      });
    }

    active_ = false;
    return core::Result<bool>::Success(true);
  }

  ~WriteTransaction() {
    if (!active_) {
      return;
    }
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    active_ = false;
  }

 private:
  sqlite3* db_{nullptr};
  bool active_{false};
};

std::string MovementTypeToStorage(const core::StockMovementType type) {
  switch (type) {
    case core::StockMovementType::Receipt:
      return "receipt";
    case core::StockMovementType::Issue:
      return "issue";
    case core::StockMovementType::Adjustment:
      return "adjustment";
  }
  return "receipt";
}

std::int64_t MovementToDelta(const core::StockMovement& movement) {
  if (movement.movement_type == core::StockMovementType::Receipt) {
    return movement.quantity;
  }
  if (movement.movement_type == core::StockMovementType::Issue) {
    return -movement.quantity;
  }
  return movement.quantity;
}

core::StockBalance BuildStockBalance(
  const std::int64_t product_id,
  std::string warehouse_code,
  const std::int64_t on_hand,
  const std::int64_t reserved,
  const std::int64_t reorder_point) {
  core::StockBalance balance;
  balance.product_id = product_id;
  balance.warehouse_code = std::move(warehouse_code);
  balance.on_hand = on_hand;
  balance.reserved = reserved;
  balance.available = on_hand - reserved;
  balance.below_reorder_point = balance.available < reorder_point;
  return balance;
}

}  // namespace

SQLiteInventoryRepository::SQLiteInventoryRepository(std::string database_path)
    : database_path_(std::move(database_path)) {
  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
  const int rc = sqlite3_open_v2(database_path_.c_str(), &db_, flags, nullptr);
  if (rc != SQLITE_OK) {
    open_error_ = db_ == nullptr ? "Failed to open SQLite database." : sqlite3_errmsg(db_);
    if (db_ != nullptr) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    return;
  }

  sqlite3_busy_timeout(db_, 5000);
  if (!ConfigureDatabase(db_, &open_error_)) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

SQLiteInventoryRepository::~SQLiteInventoryRepository() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

core::Result<bool> SQLiteInventoryRepository::EnsureOpen() const {
  if (db_ != nullptr) {
    return core::Result<bool>::Success(true);
  }

  return core::Result<bool>::Failure(core::Error{
    core::ErrorCode::RepositoryError,
    "SQLite connection is not available.",
    { open_error_.empty() ? "Unknown open error." : open_error_ }
  });
}

core::Result<bool> SQLiteInventoryRepository::Migrate() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return open_result;
  }
  return RunMigrations(db_);
}

core::Result<core::StockBalance> SQLiteInventoryRepository::GetBalance(
  const std::int64_t product_id,
  const std::string& warehouse_code) const {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<core::StockBalance>::Failure(open_result.error());
  }

  const char* sql = R"sql(
    SELECT
      COALESCE(sb.on_hand, 0),
      COALESCE(sb.reserved, 0),
      p.reorder_point
    FROM products p
    LEFT JOIN stock_balances sb ON sb.product_id = p.id AND sb.warehouse_code = ?1
    WHERE p.id = ?2;
  )sql";

  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::StockBalance>::Failure(
      SqliteError(db_, "Failed to prepare statement for GetBalance()."));
  }
  Statement statement(raw_statement);

  sqlite3_bind_text(statement.get(), 1, warehouse_code.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 2, product_id);

  const int rc = sqlite3_step(statement.get());
  if (rc == SQLITE_DONE) {
    return core::Result<core::StockBalance>::Failure(core::Error{
      core::ErrorCode::NotFound,
      "Product was not found.",
      { "id=" + std::to_string(product_id) }
    });
  }
  if (rc != SQLITE_ROW) {
    return core::Result<core::StockBalance>::Failure(
      SqliteError(db_, "Failed to execute statement in GetBalance()."));
  }

  const auto on_hand = sqlite3_column_int64(statement.get(), 0);
  const auto reserved = sqlite3_column_int64(statement.get(), 1);
  const auto reorder_point = sqlite3_column_int64(statement.get(), 2);
  return core::Result<core::StockBalance>::Success(
    BuildStockBalance(product_id, warehouse_code, on_hand, reserved, reorder_point));
}

core::Result<core::StockBalance> SQLiteInventoryRepository::ApplyMovement(const core::StockMovement& movement) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<core::StockBalance>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in ApplyMovement().");
  if (!begin_result.ok()) {
    return core::Result<core::StockBalance>::Failure(begin_result.error());
  }

  {
    const char* ensure_warehouse_sql =
      "INSERT OR IGNORE INTO warehouses(code, name, allow_negative_stock) VALUES (?1, ?2, 0);";
    sqlite3_stmt* raw_statement = nullptr;
    if (sqlite3_prepare_v2(db_, ensure_warehouse_sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to prepare ensure warehouse statement."));
    }
    Statement statement(raw_statement);
    sqlite3_bind_text(statement.get(), 1, movement.warehouse_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 2, movement.warehouse_code.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to execute ensure warehouse statement."));
    }
  }

  {
    const char* ensure_balance_sql =
      "INSERT OR IGNORE INTO stock_balances(product_id, warehouse_code, on_hand, reserved) VALUES (?1, ?2, 0, 0);";
    sqlite3_stmt* raw_statement = nullptr;
    if (sqlite3_prepare_v2(db_, ensure_balance_sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to prepare ensure stock balance statement."));
    }
    Statement statement(raw_statement);
    sqlite3_bind_int64(statement.get(), 1, movement.product_id);
    sqlite3_bind_text(statement.get(), 2, movement.warehouse_code.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to execute ensure stock balance statement."));
    }
  }

  std::int64_t current_on_hand = 0;
  std::int64_t current_reserved = 0;
  std::int64_t reorder_point = 0;
  int allow_negative_stock = 0;

  {
    const char* read_sql = R"sql(
      SELECT sb.on_hand, sb.reserved, w.allow_negative_stock, p.reorder_point
      FROM stock_balances sb
      JOIN warehouses w ON w.code = sb.warehouse_code
      JOIN products p ON p.id = sb.product_id
      WHERE sb.product_id = ?1 AND sb.warehouse_code = ?2;
    )sql";
    sqlite3_stmt* raw_statement = nullptr;
    if (sqlite3_prepare_v2(db_, read_sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to prepare stock read statement."));
    }
    Statement statement(raw_statement);
    sqlite3_bind_int64(statement.get(), 1, movement.product_id);
    sqlite3_bind_text(statement.get(), 2, movement.warehouse_code.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to read current stock state."));
    }

    current_on_hand = sqlite3_column_int64(statement.get(), 0);
    current_reserved = sqlite3_column_int64(statement.get(), 1);
    allow_negative_stock = sqlite3_column_int(statement.get(), 2);
    reorder_point = sqlite3_column_int64(statement.get(), 3);
  }

  const auto delta = MovementToDelta(movement);
  const auto new_on_hand = current_on_hand + delta;

  if (allow_negative_stock == 0 && new_on_hand < 0) {
    return core::Result<core::StockBalance>::Failure(core::Error{
      core::ErrorCode::ValidationFailed,
      "Stock movement would result in negative on_hand.",
      { "product_id=" + std::to_string(movement.product_id), "warehouse_code=" + movement.warehouse_code }
    });
  }

  {
    const char* update_sql = "UPDATE stock_balances SET on_hand = ?1 WHERE product_id = ?2 AND warehouse_code = ?3;";
    sqlite3_stmt* raw_statement = nullptr;
    if (sqlite3_prepare_v2(db_, update_sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to prepare stock update statement."));
    }
    Statement statement(raw_statement);
    sqlite3_bind_int64(statement.get(), 1, new_on_hand);
    sqlite3_bind_int64(statement.get(), 2, movement.product_id);
    sqlite3_bind_text(statement.get(), 3, movement.warehouse_code.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to execute stock update statement."));
    }
  }

  {
    const char* ledger_sql = R"sql(
      INSERT INTO stock_ledger(product_id, warehouse_code, movement_type, quantity_delta, reason)
      VALUES (?1, ?2, ?3, ?4, ?5);
    )sql";
    sqlite3_stmt* raw_statement = nullptr;
    if (sqlite3_prepare_v2(db_, ledger_sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to prepare stock ledger statement."));
    }
    Statement statement(raw_statement);
    const auto movement_type = MovementTypeToStorage(movement.movement_type);
    sqlite3_bind_int64(statement.get(), 1, movement.product_id);
    sqlite3_bind_text(statement.get(), 2, movement.warehouse_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 3, movement_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 4, delta);
    sqlite3_bind_text(statement.get(), 5, movement.reason.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
      return core::Result<core::StockBalance>::Failure(
        SqliteError(db_, "Failed to execute stock ledger statement."));
    }
  }

  auto commit_result = transaction.Commit("Failed to commit stock movement transaction.");
  if (!commit_result.ok()) {
    return core::Result<core::StockBalance>::Failure(commit_result.error());
  }

  return core::Result<core::StockBalance>::Success(
    BuildStockBalance(movement.product_id, movement.warehouse_code, new_on_hand, current_reserved, reorder_point));
}

}  // namespace infra::sqlite
