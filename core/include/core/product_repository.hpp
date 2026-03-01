#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/product.hpp"
#include "core/result.hpp"

namespace core {

struct ListProductsQuery {
  std::int32_t page{1};
  std::int32_t page_size{20};
  std::string search;
};

struct ListProductsResult {
  std::vector<Product> products;
  std::int64_t total{0};
  std::int32_t page{1};
  std::int32_t page_size{20};
  std::string search;
};

class IProductRepository {
 public:
  virtual ~IProductRepository() = default;

  virtual Result<ListProductsResult> List(const ListProductsQuery& query) const = 0;
  virtual Result<std::optional<Product>> GetById(std::int64_t id) const = 0;
  virtual Result<Product> Create(const Product& product) = 0;
  virtual Result<bool> Update(std::int64_t id, const Product& product) = 0;
  virtual Result<bool> Delete(std::int64_t id) = 0;
};

}  // namespace core
