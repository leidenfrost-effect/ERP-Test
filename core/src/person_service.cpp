#include "core/person_service.hpp"

#include <string>
#include <utility>

#include "core/error.hpp"
#include "core/person_validator.hpp"

namespace core {
namespace {

Error InvalidIdError() {
  return Error{
    ErrorCode::ValidationFailed,
    "id must be greater than zero.",
    {}
  };
}

Error ValidationError(std::vector<std::string> details) {
  return Error{
    ErrorCode::ValidationFailed,
    "Person validation failed.",
    std::move(details)
  };
}

Error NotFoundError(std::int64_t id) {
  return Error{
    ErrorCode::NotFound,
    "Person was not found.",
    { "id=" + std::to_string(id) }
  };
}

Error InvalidPaginationError(const std::string& message) {
  return Error{
    ErrorCode::ValidationFailed,
    message,
    {}
  };
}

}  // namespace

PersonService::PersonService(IPersonRepository& repository) : repository_(repository) {}

Result<ListPersonsResult> PersonService::ListPersons(
  const std::int32_t page,
  const std::int32_t page_size,
  std::string search) const {
  if (page <= 0) {
    return Result<ListPersonsResult>::Failure(
      InvalidPaginationError("page must be greater than zero."));
  }
  if (page_size <= 0 || page_size > 100) {
    return Result<ListPersonsResult>::Failure(
      InvalidPaginationError("page_size must be between 1 and 100."));
  }

  ListPersonsQuery query;
  query.page = page;
  query.page_size = page_size;
  query.search = std::move(search);
  return repository_.List(query);
}

Result<Person> PersonService::GetPersonById(const std::int64_t id) const {
  if (id <= 0) {
    return Result<Person>::Failure(InvalidIdError());
  }

  auto result = repository_.GetById(id);
  if (!result.ok()) {
    return Result<Person>::Failure(result.error());
  }
  if (!result.value().has_value()) {
    return Result<Person>::Failure(NotFoundError(id));
  }
  return Result<Person>::Success(*result.value());
}

Result<Person> PersonService::CreatePerson(const Person& person) const {
  Person candidate = person;
  candidate.id = 0;

  auto validation = ValidatePersonInput(candidate);
  if (!validation.empty()) {
    return Result<Person>::Failure(ValidationError(std::move(validation)));
  }

  return repository_.Create(candidate);
}

Result<Person> PersonService::UpdatePerson(const std::int64_t id, const Person& person) const {
  if (id <= 0) {
    return Result<Person>::Failure(InvalidIdError());
  }

  Person candidate = person;
  candidate.id = id;

  auto validation = ValidatePersonInput(candidate);
  if (!validation.empty()) {
    return Result<Person>::Failure(ValidationError(std::move(validation)));
  }

  auto update_result = repository_.Update(id, candidate);
  if (!update_result.ok()) {
    return Result<Person>::Failure(update_result.error());
  }
  if (!update_result.value()) {
    return Result<Person>::Failure(NotFoundError(id));
  }

  auto get_result = repository_.GetById(id);
  if (!get_result.ok()) {
    return Result<Person>::Failure(get_result.error());
  }
  if (!get_result.value().has_value()) {
    return Result<Person>::Failure(NotFoundError(id));
  }

  return Result<Person>::Success(*get_result.value());
}

Result<bool> PersonService::DeletePerson(const std::int64_t id) const {
  if (id <= 0) {
    return Result<bool>::Failure(InvalidIdError());
  }

  auto delete_result = repository_.Delete(id);
  if (!delete_result.ok()) {
    return Result<bool>::Failure(delete_result.error());
  }
  if (!delete_result.value()) {
    return Result<bool>::Failure(NotFoundError(id));
  }
  return Result<bool>::Success(true);
}

}  // namespace core
