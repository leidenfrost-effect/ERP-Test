#pragma once

#include <cstdint>
#include <string>

#include "core/person_repository.hpp"
#include "core/result.hpp"

namespace core {

class PersonService {
 public:
  explicit PersonService(IPersonRepository& repository);

  Result<ListPersonsResult> ListPersons(
    std::int32_t page = 1,
    std::int32_t page_size = 20,
    std::string search = "") const;
  Result<Person> GetPersonById(std::int64_t id) const;
  Result<Person> CreatePerson(const Person& person) const;
  Result<Person> UpdatePerson(std::int64_t id, const Person& person) const;
  Result<bool> DeletePerson(std::int64_t id) const;

 private:
  IPersonRepository& repository_;
};

}  // namespace core
