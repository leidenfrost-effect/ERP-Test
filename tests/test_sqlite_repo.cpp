#include <chrono>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "infra/sqlite/sqlite_person_repository.hpp"
#include "test_helpers.hpp"

namespace {

std::string BuildTempDbPath() {
  const auto temp_dir = std::filesystem::temp_directory_path();
  const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  return (temp_dir / ("hybrid_person_repo_test_" + std::to_string(timestamp) + ".db")).string();
}

}  // namespace

int main() {
  const std::string db_path = BuildTempDbPath();

  {
    infra::sqlite::SQLitePersonRepository repo(db_path);
    auto migration = repo.Migrate();
    TEST_CHECK(migration.ok());

    core::Person person;
    person.first_name = "Grace";
    person.last_name = "Hopper";
    person.email = "grace@example.com";
    person.age = 45;

    auto created = repo.Create(person);
    TEST_CHECK(created.ok());
    TEST_CHECK(created.value().id > 0);

    core::Person person_2;
    person_2.first_name = "Alan";
    person_2.last_name = "Turing";
    person_2.email = "alan@example.com";
    person_2.age = 41;
    auto created_2 = repo.Create(person_2);
    TEST_CHECK(created_2.ok());

    core::ListPersonsQuery list_query;
    list_query.page = 1;
    list_query.page_size = 1;
    list_query.search = "grace";

    auto list = repo.List(list_query);
    TEST_CHECK(list.ok());
    TEST_CHECK(list.value().total == 1);
    TEST_CHECK(list.value().persons.size() == 1);
    TEST_CHECK(list.value().persons.front().email == "grace@example.com");

    core::ListPersonsQuery second_page_query;
    second_page_query.page = 2;
    second_page_query.page_size = 1;

    auto second_page = repo.List(second_page_query);
    TEST_CHECK(second_page.ok());
    TEST_CHECK(second_page.value().total == 2);
    TEST_CHECK(second_page.value().persons.size() == 1);

    auto fetched = repo.GetById(created.value().id);
    TEST_CHECK(fetched.ok());
    TEST_CHECK(fetched.value().has_value());
    TEST_CHECK(fetched.value()->email == "grace@example.com");

    core::Person to_update = *fetched.value();
    to_update.first_name = "Rear Admiral Grace";
    to_update.email = "rearadmiral.grace@example.com";
    auto update = repo.Update(created.value().id, to_update);
    TEST_CHECK(update.ok());
    TEST_CHECK(update.value());

    auto fetched_after_update = repo.GetById(created.value().id);
    TEST_CHECK(fetched_after_update.ok());
    TEST_CHECK(fetched_after_update.value().has_value());
    TEST_CHECK(fetched_after_update.value()->first_name == "Rear Admiral Grace");

    auto deleted = repo.Delete(created.value().id);
    TEST_CHECK(deleted.ok());
    TEST_CHECK(deleted.value());

    auto fetched_after_delete = repo.GetById(created.value().id);
    TEST_CHECK(fetched_after_delete.ok());
    TEST_CHECK(!fetched_after_delete.value().has_value());
  }

  std::error_code ec;
  std::filesystem::remove(db_path, ec);

  return 0;
}
