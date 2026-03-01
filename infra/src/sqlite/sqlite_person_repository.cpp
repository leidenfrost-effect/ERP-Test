#include "infra/sqlite/sqlite_person_repository.hpp"

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

core::Person ReadPerson(sqlite3_stmt* statement) {
  core::Person person;
  person.id = sqlite3_column_int64(statement, 0);

  const auto* first_name = sqlite3_column_text(statement, 1);
  const auto* last_name = sqlite3_column_text(statement, 2);
  const auto* email = sqlite3_column_text(statement, 3);

  person.first_name = first_name == nullptr ? "" : reinterpret_cast<const char*>(first_name);
  person.last_name = last_name == nullptr ? "" : reinterpret_cast<const char*>(last_name);
  person.email = email == nullptr ? "" : reinterpret_cast<const char*>(email);
  person.age = sqlite3_column_int(statement, 4);

  return person;
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

}  // namespace

SQLitePersonRepository::SQLitePersonRepository(std::string database_path)
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

SQLitePersonRepository::~SQLitePersonRepository() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

core::Result<bool> SQLitePersonRepository::EnsureOpen() const {
  if (db_ != nullptr) {
    return core::Result<bool>::Success(true);
  }

  return core::Result<bool>::Failure(core::Error{
    core::ErrorCode::RepositoryError,
    "SQLite connection is not available.",
    { open_error_.empty() ? "Unknown open error." : open_error_ }
  });
}

core::Result<bool> SQLitePersonRepository::Migrate() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return open_result;
  }
  return RunMigrations(db_);
}

core::Result<core::ListPersonsResult> SQLitePersonRepository::List(const core::ListPersonsQuery& query) const {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<core::ListPersonsResult>::Failure(open_result.error());
  }

  const std::int32_t page = std::max<std::int32_t>(1, query.page);
  const std::int32_t page_size = std::clamp<std::int32_t>(query.page_size, 1, 100);
  const std::int32_t offset = (page - 1) * page_size;

  const std::string search = NormalizeSearch(query.search);
  const std::string like_pattern = "%" + search + "%";

  const char* count_sql = R"sql(
    SELECT COUNT(*)
    FROM persons
    WHERE (?1 = '' OR lower(first_name) LIKE ?2 OR lower(last_name) LIKE ?2 OR lower(email) LIKE ?2);
  )sql";
  sqlite3_stmt* raw_count_statement = nullptr;
  if (sqlite3_prepare_v2(db_, count_sql, -1, &raw_count_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::ListPersonsResult>::Failure(
      SqliteError(db_, "Failed to prepare count statement for List()."));
  }
  Statement count_statement(raw_count_statement);

  sqlite3_bind_text(count_statement.get(), 1, search.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(count_statement.get(), 2, like_pattern.c_str(), -1, SQLITE_TRANSIENT);

  const int count_rc = sqlite3_step(count_statement.get());
  if (count_rc != SQLITE_ROW) {
    return core::Result<core::ListPersonsResult>::Failure(
      SqliteError(db_, "Failed to execute count statement in List()."));
  }
  const std::int64_t total = sqlite3_column_int64(count_statement.get(), 0);

  const char* list_sql = R"sql(
    SELECT id, first_name, last_name, email, age
    FROM persons
    WHERE (?1 = '' OR lower(first_name) LIKE ?2 OR lower(last_name) LIKE ?2 OR lower(email) LIKE ?2)
    ORDER BY id ASC
    LIMIT ?3 OFFSET ?4;
  )sql";
  sqlite3_stmt* raw_list_statement = nullptr;
  if (sqlite3_prepare_v2(db_, list_sql, -1, &raw_list_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::ListPersonsResult>::Failure(
      SqliteError(db_, "Failed to prepare list statement for List()."));
  }
  Statement list_statement(raw_list_statement);

  sqlite3_bind_text(list_statement.get(), 1, search.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(list_statement.get(), 2, like_pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(list_statement.get(), 3, page_size);
  sqlite3_bind_int(list_statement.get(), 4, offset);

  std::vector<core::Person> persons;

  int rc = SQLITE_OK;
  while ((rc = sqlite3_step(list_statement.get())) == SQLITE_ROW) {
    persons.emplace_back(ReadPerson(list_statement.get()));
  }

  if (rc != SQLITE_DONE) {
    return core::Result<core::ListPersonsResult>::Failure(
      SqliteError(db_, "Failed while reading rows in List()."));
  }

  core::ListPersonsResult result;
  result.persons = std::move(persons);
  result.total = total;
  result.page = page;
  result.page_size = page_size;
  result.search = search;
  return core::Result<core::ListPersonsResult>::Success(std::move(result));
}

core::Result<std::optional<core::Person>> SQLitePersonRepository::GetById(const std::int64_t id) const {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<std::optional<core::Person>>::Failure(open_result.error());
  }

  const char* sql = "SELECT id, first_name, last_name, email, age FROM persons WHERE id = ?;";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<std::optional<core::Person>>::Failure(
      SqliteError(db_, "Failed to prepare statement for GetById()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_int64(statement.get(), 1, id);

  const int rc = sqlite3_step(statement.get());
  if (rc == SQLITE_ROW) {
    return core::Result<std::optional<core::Person>>::Success(ReadPerson(statement.get()));
  }
  if (rc == SQLITE_DONE) {
    return core::Result<std::optional<core::Person>>::Success(std::nullopt);
  }

  return core::Result<std::optional<core::Person>>::Failure(
    SqliteError(db_, "Failed to execute statement in GetById()."));
}

core::Result<core::Person> SQLitePersonRepository::Create(const core::Person& person) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<core::Person>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in Create().");
  if (!begin_result.ok()) {
    return core::Result<core::Person>::Failure(begin_result.error());
  }

  const char* sql = "INSERT INTO persons(first_name, last_name, email, age) VALUES(?, ?, ?, ?);";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<core::Person>::Failure(
      SqliteError(db_, "Failed to prepare statement for Create()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_text(statement.get(), 1, person.first_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, person.last_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, person.email.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 4, person.age);

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    return core::Result<core::Person>::Failure(
      SqliteError(db_, "Failed to execute INSERT in Create()."));
  }

  auto commit_result = transaction.Commit("Failed to commit transaction in Create().");
  if (!commit_result.ok()) {
    return core::Result<core::Person>::Failure(commit_result.error());
  }

  core::Person created = person;
  created.id = sqlite3_last_insert_rowid(db_);
  return core::Result<core::Person>::Success(std::move(created));
}

core::Result<bool> SQLitePersonRepository::Update(const std::int64_t id, const core::Person& person) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<bool>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in Update().");
  if (!begin_result.ok()) {
    return core::Result<bool>::Failure(begin_result.error());
  }

  const char* sql = "UPDATE persons SET first_name = ?, last_name = ?, email = ?, age = ? WHERE id = ?;";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<bool>::Failure(SqliteError(db_, "Failed to prepare statement for Update()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_text(statement.get(), 1, person.first_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, person.last_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, person.email.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 4, person.age);
  sqlite3_bind_int64(statement.get(), 5, id);

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    return core::Result<bool>::Failure(SqliteError(db_, "Failed to execute UPDATE in Update()."));
  }

  const bool updated = sqlite3_changes(db_) > 0;
  auto commit_result = transaction.Commit("Failed to commit transaction in Update().");
  if (!commit_result.ok()) {
    return core::Result<bool>::Failure(commit_result.error());
  }

  return core::Result<bool>::Success(updated);
}

core::Result<bool> SQLitePersonRepository::Delete(const std::int64_t id) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  auto open_result = EnsureOpen();
  if (!open_result.ok()) {
    return core::Result<bool>::Failure(open_result.error());
  }

  WriteTransaction transaction(db_);
  auto begin_result = transaction.Begin("Failed to begin transaction in Delete().");
  if (!begin_result.ok()) {
    return core::Result<bool>::Failure(begin_result.error());
  }

  const char* sql = "DELETE FROM persons WHERE id = ?;";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &raw_statement, nullptr) != SQLITE_OK) {
    return core::Result<bool>::Failure(SqliteError(db_, "Failed to prepare statement for Delete()."));
  }

  Statement statement(raw_statement);
  sqlite3_bind_int64(statement.get(), 1, id);

  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    return core::Result<bool>::Failure(SqliteError(db_, "Failed to execute DELETE in Delete()."));
  }

  const bool deleted = sqlite3_changes(db_) > 0;
  auto commit_result = transaction.Commit("Failed to commit transaction in Delete().");
  if (!commit_result.ok()) {
    return core::Result<bool>::Failure(commit_result.error());
  }

  return core::Result<bool>::Success(deleted);
}

}  // namespace infra::sqlite
