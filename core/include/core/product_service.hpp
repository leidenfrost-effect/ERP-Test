#pragma once

#include <cstdint>
#include <string>

#include "core/product_repository.hpp"
#include "core/result.hpp"

namespace core {

class ProductService {
 public:
  explicit ProductService(IProductRepository& repository);

  Result<ListProductsResult> ListProducts(
    std::int32_t page = 1,
    std::int32_t page_size = 20,
    std::string search = "") const;
  Result<Product> GetProductById(std::int64_t id) const;
  Result<Product> CreateProduct(const Product& product) const;
  Result<Product> UpdateProduct(std::int64_t id, const Product& product) const;
  Result<bool> DeleteProduct(std::int64_t id) const;

 private:
  IProductRepository& repository_;
};

}  // namespace core
