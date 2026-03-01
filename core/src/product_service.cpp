#include "core/product_service.hpp"

#include <string>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/product_validator.hpp"

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
    "Product validation failed.",
    std::move(details)
  };
}

Error NotFoundError(std::int64_t id) {
  return Error{
    ErrorCode::NotFound,
    "Product was not found.",
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

ProductService::ProductService(IProductRepository& repository) : repository_(repository) {}

Result<ListProductsResult> ProductService::ListProducts(
  const std::int32_t page,
  const std::int32_t page_size,
  std::string search) const {
  if (page <= 0) {
    return Result<ListProductsResult>::Failure(
      InvalidPaginationError("page must be greater than zero."));
  }
  if (page_size <= 0 || page_size > 100) {
    return Result<ListProductsResult>::Failure(
      InvalidPaginationError("page_size must be between 1 and 100."));
  }

  ListProductsQuery query;
  query.page = page;
  query.page_size = page_size;
  query.search = std::move(search);
  return repository_.List(query);
}

Result<Product> ProductService::GetProductById(const std::int64_t id) const {
  if (id <= 0) {
    return Result<Product>::Failure(InvalidIdError());
  }

  auto result = repository_.GetById(id);
  if (!result.ok()) {
    return Result<Product>::Failure(result.error());
  }
  if (!result.value().has_value()) {
    return Result<Product>::Failure(NotFoundError(id));
  }
  return Result<Product>::Success(*result.value());
}

Result<Product> ProductService::CreateProduct(const Product& product) const {
  Product candidate = product;
  candidate.id = 0;

  auto validation = ValidateProductInput(candidate);
  if (!validation.empty()) {
    return Result<Product>::Failure(ValidationError(std::move(validation)));
  }

  return repository_.Create(candidate);
}

Result<Product> ProductService::UpdateProduct(const std::int64_t id, const Product& product) const {
  if (id <= 0) {
    return Result<Product>::Failure(InvalidIdError());
  }

  Product candidate = product;
  candidate.id = id;

  auto validation = ValidateProductInput(candidate);
  if (!validation.empty()) {
    return Result<Product>::Failure(ValidationError(std::move(validation)));
  }

  auto update_result = repository_.Update(id, candidate);
  if (!update_result.ok()) {
    return Result<Product>::Failure(update_result.error());
  }
  if (!update_result.value()) {
    return Result<Product>::Failure(NotFoundError(id));
  }

  auto get_result = repository_.GetById(id);
  if (!get_result.ok()) {
    return Result<Product>::Failure(get_result.error());
  }
  if (!get_result.value().has_value()) {
    return Result<Product>::Failure(NotFoundError(id));
  }

  return Result<Product>::Success(*get_result.value());
}

Result<bool> ProductService::DeleteProduct(const std::int64_t id) const {
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
