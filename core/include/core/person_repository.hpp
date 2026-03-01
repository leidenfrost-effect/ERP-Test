#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/person.hpp"
#include "core/result.hpp"

namespace core {

struct ListPersonsQuery {
  std::int32_t page{1};
  std::int32_t page_size{20};
  std::string search;
};

struct ListPersonsResult {
  std::vector<Person> persons;
  std::int64_t total{0};
  std::int32_t page{1};
  std::int32_t page_size{20};
  std::string search;
};

class IPersonRepository {
 public:
  virtual ~IPersonRepository() = default;

  virtual Result<ListPersonsResult> List(const ListPersonsQuery& query) const = 0;
  virtual Result<std::optional<Person>> GetById(std::int64_t id) const = 0;
  virtual Result<Person> Create(const Person& person) = 0;
  virtual Result<bool> Update(std::int64_t id, const Person& person) = 0;
  virtual Result<bool> Delete(std::int64_t id) = 0;
};

}  // namespace core
