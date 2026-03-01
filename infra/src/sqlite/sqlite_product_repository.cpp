#include "infra/sqlite/sqlite_product_repository.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

std::string Trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\n\r");
  return value.substr(first, last - first + 1);
}

std::string NormalizeSearch(const std::string& value) {
  std::string normalized = Trim(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return normalized;
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

core::Product ReadProduct(sqlite3_stmt* statement) {
  core::Product product;
  product.id = sqlite3_column_int64(statement, 0);

  const auto* sku = sqlite3_column_text(statement, 1);
  const auto* name = sqlite3_column_text(statement, 2);
  const auto* category = sqlite3_column_text(statement, 3);
  const auto* default_uom = sqlite3_column_text(statement, 4);
  const auto* product_type = sqlite3_column_text(statement, 5);

  product.sku = sku == nullptr ? "" : reinterpret_cast<const char*>(sku);
  product.name = name == nullptr ? "" : reinterpret_cast<const char*>(name);
  product.category = category == nullptr ? "" : reinterpret_cast<const char*>(category);
  product.default_uom = default_uom == nullptr ? "EA" : reinterpret_cast<const char*>(default_uom);

  core::ProductType parsed_type = core::ProductType::Finished;
  if (product_type != nullptr) {
    core::TryParseProductType(reinterpret_cast<const char*>(product_type), &parsed_type);
  }
  product.product_type = parsed_type;

  product.is_stock_tracked = sqlite3_column_int(statement, 6) != 0;
  product.safety_stock = sqlite3_column_int(statement, 7);
  product.reorder_point = sqlite3_column_int(statement, 8);

  return product;
}

}  // namespace

SQLiteProductRepository::SQLiteProductRepository(std::string database_path)
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

SQLiteProductRepository::~SQLiteProductRepository() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

core::Result<bool> SQLiteProductRepository::EnsureOpen() const {
  if (db_ != nullptr) {
    return core::Result<bool>::Success(true);
  }

  return core::Result<bool>::Failure(core::Error{
    core::ErrorCode::RepositoryError,
    "SQLite connection is not available.",
    { open_error_.empty() ? "Unknown open error." : open_error_ }
  });
}

core::Result<bool> SQLiteProductRepository::Migrate() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return open_result;
  }
  return RunMigrations(db_);
}

core::Result<core::ListProductsResult> SQLiteProductRepository::List(const core::ListProductsQuery& query) const {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<core::ListProductsResult>::Failure(open_result.error());
  }

  const std::int32_t page = std::max<std::int32_t>(1, query.page);
  const std::int32_t page_size = std::clamp<std::int32_t>(query.page_size, 1, 100);
  const std::int32_t offset = (page - 1) * page_size;

  const std::string search = NormalizeSearch(query.search);
  const std::string like_pattern = "%" + search + "%";

  const char* count_sql = R"sql(
    SELECT COUNT(*)
    FROM products
    WHERE (?1 = '' OR lower(sku) LIKE ?2 OR lower(name) LIKE ?2 OR lower(category) LIKE ?2);
  )sql";
  sqlite3_stmt* raw_count_statement = nullptr;
  if (sqlite3_prepare_v2(db_, count_sql, -1, &raw_count_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::ListProductsResult>::Failure(
      SqliteError(db_, "Failed to prepare count statement for product list."));
  }
  Statement count_statement(raw_count_statement);

  sqlite3_bind_text(count_statement.get(), 1, search.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(count_statement.get(), 2, like_pattern.c_str(), -1, SQLITE_TRANSIENT);

  const int count_rc = sqlite3_step(count_statement.get());
  if (count_rc != SQLITE_ROW) {
    return core::Result<core::ListProductsResult>::Failure(
      SqliteError(db_, "Failed to execute count statement for product list."));
  }
  const std::int64_t total = sqlite3_column_int64(count_statement.get(), 0);

  const char* list_sql = R"sql(
    SELECT id, sku, name, category, default_uom, product_type, is_stock_tracked, safety_stock, reorder_point
    FROM products
    WHERE (?1 = '' OR lower(sku) LIKE ?2 OR lower(name) LIKE ?2 OR lower(category) LIKE ?2)
    ORDER BY id ASC
    LIMIT ?3 OFFSET ?4;
  )sql";
  sqlite3_stmt* raw_list_statement = nullptr;
  if (sqlite3_prepare_v2(db_, list_sql, -1, &raw_list_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::ListProductsResult>::Failure(
      SqliteError(db_, "Failed to prepare list statement for product list."));
  }
  Statement list_statement(raw_list_statement);

  sqlite3_bind_text(list_statement.get(), 1, search.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(list_statement.get(), 2, like_pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(list_statement.get(), 3, page_size);
  sqlite3_bind_int(list_statement.get(), 4, offset);

  std::vector<core::Product> products;
  int rc = SQLITE_OK;
  while ((rc = sqlite3_step(list_statement.get())) == SQLITE_ROW) {
    products.emplace_back(ReadProduct(list_statement.get()));
  }
  if (rc != SQLITE_DONE) {
    return core::Result<core::ListProductsResult>::Failure(
      SqliteError(db_, "Failed while reading rows in product list."));
  }

  core::ListProductsResult result;
  result.products = std::move(products);
  result.total = total;
  result.page = page;
  result.page_size = page_size;
  result.search = search;
  return core::Result<core::ListProductsResult>::Success(std::move(result));
}

core::Result<std::optional<core::Product>> SQLiteProductRepository::GetById(const std::int64_t id) const {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<std::optional<core::Product>>::Failure(open_result.error());
  }

  const char* sql = R"sql(
    SELECT id, sku, name, category, default_uom, product_type, is_stock_tracked, safety_stock, reorder_point
    FROM products
    WHERE id = ?;
  )sql";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<std::optional<core::Product>>::Failure(
      SqliteError(db_, "Failed to prepare statement for GetProductById()."));
  }
  Statement statement(raw_statement);

  sqlite3_bind_int64(statement.get(), 1, id);

  const int rc = sqlite3_step(statement.get());
  if (rc == SQLITE_ROW) {
    return core::Result<std::optional<core::Product>>::Success(ReadProduct(statement.get()));
  }
  if (rc == SQLITE_DONE) {
    return core::Result<std::optional<core::Product>>::Success(std::nullopt);
  }

  return core::Result<std::optional<core::Product>>::Failure(
    SqliteError(db_, "Failed to execute statement in GetProductById()."));
}

core::Result<core::Product> SQLiteProductRepository::Create(const core::Product& product) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<core::Product>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in CreateProduct().");
  if (!begin_result.ok()) {
    return core::Result<core::Product>::Failure(begin_result.error());
  }

  const char* sql = R"sql(
    INSERT INTO products(
      sku, name, category, default_uom, product_type, is_stock_tracked, safety_stock, reorder_point
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?);
  )sql";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::Product>::Failure(
      SqliteError(db_, "Failed to prepare statement for CreateProduct()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_text(statement.get(), 1, product.sku.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, product.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, product.category.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 4, product.default_uom.c_str(), -1, SQLITE_TRANSIENT);
  const auto product_type = core::ProductTypeToStorage(product.product_type);
  sqlite3_bind_text(statement.get(), 5, product_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 6, product.is_stock_tracked ? 1 : 0);
  sqlite3_bind_int(statement.get(), 7, product.safety_stock);
  sqlite3_bind_int(statement.get(), 8, product.reorder_point);

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    return core::Result<core::Product>::Failure(
      SqliteError(db_, "Failed to execute INSERT in CreateProduct()."));
  }

  auto commit_result = transaction.Commit("Failed to commit transaction in CreateProduct().");
  if (!commit_result.ok()) {
    return core::Result<core::Product>::Failure(commit_result.error());
  }

  core::Product created = product;
  created.id = sqlite3_last_insert_rowid(db_);
  return core::Result<core::Product>::Success(std::move(created));
}

core::Result<bool> SQLiteProductRepository::Update(const std::int64_t id, const core::Product& product) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<bool>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in UpdateProduct().");
  if (!begin_result.ok()) {
    return core::Result<bool>::Failure(begin_result.error());
  }

  const char* sql = R"sql(
    UPDATE products
    SET sku = ?, name = ?, category = ?, default_uom = ?, product_type = ?, is_stock_tracked = ?,
        safety_stock = ?, reorder_point = ?
    WHERE id = ?;
  )sql";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<bool>::Failure(
      SqliteError(db_, "Failed to prepare statement for UpdateProduct()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_text(statement.get(), 1, product.sku.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, product.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, product.category.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 4, product.default_uom.c_str(), -1, SQLITE_TRANSIENT);
  const auto product_type = core::ProductTypeToStorage(product.product_type);
  sqlite3_bind_text(statement.get(), 5, product_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 6, product.is_stock_tracked ? 1 : 0);
  sqlite3_bind_int(statement.get(), 7, product.safety_stock);
  sqlite3_bind_int(statement.get(), 8, product.reorder_point);
  sqlite3_bind_int64(statement.get(), 9, id);

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    return core::Result<bool>::Failure(
      SqliteError(db_, "Failed to execute UPDATE in UpdateProduct()."));
  }

  const bool updated = sqlite3_changes(db_) > 0;
  auto commit_result = transaction.Commit("Failed to commit transaction in UpdateProduct().");
  if (!commit_result.ok()) {
    return core::Result<bool>::Failure(commit_result.error());
  }

  return core::Result<bool>::Success(updated);
}

core::Result<bool> SQLiteProductRepository::Delete(const std::int64_t id) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<bool>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in DeleteProduct().");
  if (!begin_result.ok()) {
    return core::Result<bool>::Failure(begin_result.error());
  }

  const char* sql = "DELETE FROM products WHERE id = ?;";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<bool>::Failure(
      SqliteError(db_, "Failed to prepare statement for DeleteProduct()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_int64(statement.get(), 1, id);

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    return core::Result<bool>::Failure(
      SqliteError(db_, "Failed to execute DELETE in DeleteProduct()."));
  }

  const bool deleted = sqlite3_changes(db_) > 0;
  auto commit_result = transaction.Commit("Failed to commit transaction in DeleteProduct().");
  if (!commit_result.ok()) {
    return core::Result<bool>::Failure(commit_result.error());
  }

  return core::Result<bool>::Success(deleted);
}

}  // namespace infra::sqlite
