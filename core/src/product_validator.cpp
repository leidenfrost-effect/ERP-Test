#include "core/product_validator.hpp"

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

bool IsValidSku(const std::string& sku) {
  static const std::regex kPattern(R"(^[A-Za-z0-9][A-Za-z0-9._-]{1,63}$)");
  return std::regex_match(sku, kPattern);
}

}  // namespace

std::vector<std::string> ValidateProductInput(const Product& product) {
  std::vector<std::string> errors;

  if (!IsValidSku(product.sku)) {
    errors.emplace_back("sku must match [A-Za-z0-9][A-Za-z0-9._-]{1,63}.");
  }
  if (Trim(product.name).empty()) {
    errors.emplace_back("name must not be empty.");
  }
  if (product.name.size() > 200) {
    errors.emplace_back("name length must be <= 200.");
  }
  if (Trim(product.category).empty()) {
    errors.emplace_back("category must not be empty.");
  }
  if (product.category.size() > 100) {
    errors.emplace_back("category length must be <= 100.");
  }
  if (Trim(product.default_uom).empty()) {
    errors.emplace_back("default_uom must not be empty.");
  }
  if (product.default_uom.size() > 16) {
    errors.emplace_back("default_uom length must be <= 16.");
  }
  if (product.safety_stock < 0) {
    errors.emplace_back("safety_stock must be >= 0.");
  }
  if (product.reorder_point < 0) {
    errors.emplace_back("reorder_point must be >= 0.");
  }

  return errors;
}

}  // namespace core
