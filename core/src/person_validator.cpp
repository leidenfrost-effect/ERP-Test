#include "core/person_validator.hpp"

#include <regex>

namespace core {
namespace {

std::string Trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\n\r");
  return value.substr(first, last - first + 1);
}

bool IsValidEmail(const std::string& email) {
  static const std::regex kPattern(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)");
  return std::regex_match(email, kPattern);
}

}  // namespace

std::vector<std::string> ValidatePersonInput(const Person& person) {
  std::vector<std::string> errors;

  if (Trim(person.first_name).empty()) {
    errors.emplace_back("first_name must not be empty.");
  }
  if (Trim(person.last_name).empty()) {
    errors.emplace_back("last_name must not be empty.");
  }
  if (person.first_name.size() > 100) {
    errors.emplace_back("first_name length must be <= 100.");
  }
  if (person.last_name.size() > 100) {
    errors.emplace_back("last_name length must be <= 100.");
  }
  if (!IsValidEmail(person.email)) {
    errors.emplace_back("email must be a valid email address.");
  }
  if (person.age < 0 || person.age > 130) {
    errors.emplace_back("age must be between 0 and 130.");
  }

  return errors;
}

}  // namespace core
