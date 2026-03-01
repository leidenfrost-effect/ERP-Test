#pragma once

#include <sqlite3.h>

#include <mutex>
#include <string>

#include "core/product_repository.hpp"

namespace infra::sqlite {

class SQLiteProductRepository : public core::IProductRepository {
 public:
  explicit SQLiteProductRepository(std::string database_path);
  ~SQLiteProductRepository() override;

  SQLiteProductRepository(const SQLiteProductRepository&) = delete;
  SQLiteProductRepository& operator=(const SQLiteProductRepository&) = delete;

  core::Result<bool> Migrate();

  core::Result<core::ListProductsResult> List(const core::ListProductsQuery& query) const override;
  core::Result<std::optional<core::Product>> GetById(std::int64_t id) const override;
  core::Result<core::Product> Create(const core::Product& product) override;
  core::Result<bool> Update(std::int64_t id, const core::Product& product) override;
  core::Result<bool> Delete(std::int64_t id) override;

 private:
  core::Result<bool> EnsureOpen() const;

  sqlite3* db_{nullptr};
  std::string database_path_;
  std::string open_error_;
  mutable std::mutex db_mutex_;
};

}  // namespace infra::sqlite
