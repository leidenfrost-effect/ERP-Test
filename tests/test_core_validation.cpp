#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/person_service.hpp"
#include "test_helpers.hpp"

namespace {

class InMemoryPersonRepository : public core::IPersonRepository {
 public:
  core::Result<core::ListPersonsResult> List(const core::ListPersonsQuery& query) const override {
    const std::int32_t page = std::max<std::int32_t>(1, query.page);
    const std::int32_t page_size = std::clamp<std::int32_t>(query.page_size, 1, 100);

    std::vector<core::Person> filtered = persons_;
    if (!query.search.empty()) {
      filtered.erase(
        std::remove_if(filtered.begin(), filtered.end(), [&](const core::Person& person) {
          return person.first_name.find(query.search) == std::string::npos &&
                 person.last_name.find(query.search) == std::string::npos &&
                 person.email.find(query.search) == std::string::npos;
        }),
        filtered.end());
    }

    const std::int64_t total = static_cast<std::int64_t>(filtered.size());
    const std::size_t offset = static_cast<std::size_t>((page - 1) * page_size);
    const std::size_t end =
      std::min<std::size_t>(filtered.size(), offset + static_cast<std::size_t>(page_size));

    std::vector<core::Person> page_items;
    if (offset < filtered.size()) {
      page_items.assign(filtered.begin() + static_cast<std::ptrdiff_t>(offset),
                        filtered.begin() + static_cast<std::ptrdiff_t>(end));
    }

    core::ListPersonsResult result;
    result.persons = std::move(page_items);
    result.total = total;
    result.page = page;
    result.page_size = page_size;
    result.search = query.search;
    return core::Result<core::ListPersonsResult>::Success(std::move(result));
  }

  core::Result<std::optional<core::Person>> GetById(const std::int64_t id) const override {
    const auto it = std::find_if(persons_.begin(), persons_.end(), [id](const core::Person& person) {
      return person.id == id;
    });
    if (it == persons_.end()) {
      return core::Result<std::optional<core::Person>>::Success(std::nullopt);
    }
    return core::Result<std::optional<core::Person>>::Success(*it);
  }

  core::Result<core::Person> Create(const core::Person& person) override {
    core::Person created = person;
    created.id = next_id_++;
    persons_.push_back(created);
    return core::Result<core::Person>::Success(created);
  }

  core::Result<bool> Update(const std::int64_t id, const core::Person& person) override {
    const auto it = std::find_if(persons_.begin(), persons_.end(), [id](const core::Person& current) {
      return current.id == id;
    });
    if (it == persons_.end()) {
      return core::Result<bool>::Success(false);
    }

    core::Person updated = person;
    updated.id = id;
    *it = updated;
    return core::Result<bool>::Success(true);
  }

  core::Result<bool> Delete(const std::int64_t id) override {
    const auto old_size = persons_.size();
    persons_.erase(
      std::remove_if(persons_.begin(), persons_.end(), [id](const core::Person& person) { return person.id == id; }),
      persons_.end());
    return core::Result<bool>::Success(persons_.size() != old_size);
  }

 private:
  std::vector<core::Person> persons_;
  std::int64_t next_id_{1};
};

}  // namespace

int main() {
  InMemoryPersonRepository repo;
  core::PersonService service(repo);

  core::Person invalid_person;
  invalid_person.first_name = "";
  invalid_person.last_name = "Lovelace";
  invalid_person.email = "not-an-email";
  invalid_person.age = 150;

  auto invalid_create = service.CreatePerson(invalid_person);
  TEST_CHECK(!invalid_create.ok());
  TEST_CHECK(invalid_create.error().code == core::ErrorCode::ValidationFailed);
  TEST_CHECK(invalid_create.error().details.size() >= 2);

  core::Person valid_person;
  valid_person.first_name = "Ada";
  valid_person.last_name = "Lovelace";
  valid_person.email = "ada@example.com";
  valid_person.age = 36;

  auto created = service.CreatePerson(valid_person);
  TEST_CHECK(created.ok());
  TEST_CHECK(created.value().id > 0);

  auto paged = service.ListPersons(1, 10, "Ada");
  TEST_CHECK(paged.ok());
  TEST_CHECK(paged.value().total == 1);
  TEST_CHECK(paged.value().persons.size() == 1);

  auto invalid_page = service.ListPersons(0, 10);
  TEST_CHECK(!invalid_page.ok());
  TEST_CHECK(invalid_page.error().code == core::ErrorCode::ValidationFailed);

  auto invalid_page_size = service.ListPersons(1, 0);
  TEST_CHECK(!invalid_page_size.ok());
  TEST_CHECK(invalid_page_size.error().code == core::ErrorCode::ValidationFailed);

  auto fetched = service.GetPersonById(created.value().id);
  TEST_CHECK(fetched.ok());
  TEST_CHECK(fetched.value().email == "ada@example.com");

  core::Person updated_input = fetched.value();
  updated_input.first_name = "Augusta Ada";
  auto updated = service.UpdatePerson(created.value().id, updated_input);
  TEST_CHECK(updated.ok());
  TEST_CHECK(updated.value().first_name == "Augusta Ada");

  auto not_found_update = service.UpdatePerson(999, updated_input);
  TEST_CHECK(!not_found_update.ok());
  TEST_CHECK(not_found_update.error().code == core::ErrorCode::NotFound);

  auto deleted = service.DeletePerson(created.value().id);
  TEST_CHECK(deleted.ok());
  TEST_CHECK(deleted.value());

  auto second_delete = service.DeletePerson(created.value().id);
  TEST_CHECK(!second_delete.ok());
  TEST_CHECK(second_delete.error().code == core::ErrorCode::NotFound);

  return 0;
}
