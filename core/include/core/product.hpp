#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace core {

enum class ProductType {
  Finished,
  Semi,
  Raw
};

inline std::string ProductTypeToStorage(ProductType type) {
  switch (type) {
    case ProductType::Finished:
      return "finished";
    case ProductType::Semi:
      return "semi";
    case ProductType::Raw:
      return "raw";
  }
  return "finished";
}

inline bool TryParseProductType(const std::string& value, ProductType* parsed) {
  if (parsed == nullptr) {
    return false;
  }

  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (normalized == "finished") {
    *parsed = ProductType::Finished;
    return true;
  }
  if (normalized == "semi") {
    *parsed = ProductType::Semi;
    return true;
  }
  if (normalized == "raw") {
    *parsed = ProductType::Raw;
    return true;
  }
  return false;
}

struct Product {
  std::int64_t id{0};
  std::string sku;
  std::string name;
  std::string category;
  std::string default_uom{"EA"};
  ProductType product_type{ProductType::Finished};
  bool is_stock_tracked{true};
  std::int32_t safety_stock{0};
  std::int32_t reorder_point{0};
};

}  // namespace core
