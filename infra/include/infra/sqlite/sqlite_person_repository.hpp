#pragma once

#include <sqlite3.h>

#include <mutex>
#include <string>

#include "core/person_repository.hpp"

namespace infra::sqlite {

class SQLitePersonRepository : public core::IPersonRepository {
 public:
  explicit SQLitePersonRepository(std::string database_path);
  ~SQLitePersonRepository() override;

  SQLitePersonRepository(const SQLitePersonRepository&) = delete;
  SQLitePersonRepository& operator=(const SQLitePersonRepository&) = delete;

  core::Result<bool> Migrate();

  core::Result<core::ListPersonsResult> List(const core::ListPersonsQuery& query) const override;
  core::Result<std::optional<core::Person>> GetById(std::int64_t id) const override;
  core::Result<core::Person> Create(const core::Person& person) override;
  core::Result<bool> Update(std::int64_t id, const core::Person& person) override;
  core::Result<bool> Delete(std::int64_t id) override;

 private:
  core::Result<bool> EnsureOpen() const;

  sqlite3* db_{nullptr};
  std::string database_path_;
  std::string open_error_;
  mutable std::mutex db_mutex_;
};

}  // namespace infra::sqlite
