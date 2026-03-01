#pragma once

#include <sqlite3.h>

#include <mutex>
#include <string>

#include "core/inventory_repository.hpp"

namespace infra::sqlite {

class SQLiteInventoryRepository : public core::IInventoryRepository {
 public:
  explicit SQLiteInventoryRepository(std::string database_path);
  ~SQLiteInventoryRepository() override;

  SQLiteInventoryRepository(const SQLiteInventoryRepository&) = delete;
  SQLiteInventoryRepository& operator=(const SQLiteInventoryRepository&) = delete;

  core::Result<bool> Migrate();

  core::Result<core::StockBalance> GetBalance(std::int64_t product_id, const std::string& warehouse_code) const override;
  core::Result<core::StockBalance> ApplyMovement(const core::StockMovement& movement) override;

 private:
  core::Result<bool> EnsureOpen() const;

  sqlite3* db_{nullptr};
  std::string database_path_;
  std::string open_error_;
  mutable std::mutex db_mutex_;
};

}  // namespace infra::sqlite
