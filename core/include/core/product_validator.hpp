#pragma once

#include <string>
#include <vector>

#include "core/product.hpp"

namespace core {

std::vector<std::string> ValidateProductInput(const Product& product);

}  // namespace core
