#pragma once

#include <string>
#include <vector>

#include "core/person.hpp"

namespace core {

std::vector<std::string> ValidatePersonInput(const Person& person);

}  // namespace core
