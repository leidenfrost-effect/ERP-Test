#include "core/inventory_service.hpp"

#include <string>
#include <utility>
#include <vector>

#include "core/error.hpp"

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

Error ValidationError(const std::string& message, std::vector<std::string> details = {}) {
  return Error{
    ErrorCode::ValidationFailed,
    message,
    std::move(details)
  };
}

Error NotFoundError(std::int64_t product_id) {
  return Error{
    ErrorCode::NotFound,
    "Product was not found.",
    { "id=" + std::to_string(product_id) }
  };
}

}  // namespace

InventoryService::InventoryService(IInventoryRepository& inventory_repository, IProductRepository& product_repository)
    : inventory_repository_(inventory_repository), product_repository_(product_repository) {}

Result<StockBalance> InventoryService::GetStockBalance(
  const std::int64_t product_id,
  std::string warehouse_code) const {
  if (product_id <= 0) {
    return Result<StockBalance>::Failure(ValidationError("product_id must be greater than zero."));
  }

  warehouse_code = Trim(warehouse_code);
  if (warehouse_code.empty()) {
    return Result<StockBalance>::Failure(ValidationError("warehouse_code must not be empty."));
  }

  auto product_result = product_repository_.GetById(product_id);
  if (!product_result.ok()) {
    return Result<StockBalance>::Failure(product_result.error());
  }
  if (!product_result.value().has_value()) {
    return Result<StockBalance>::Failure(NotFoundError(product_id));
  }

  return inventory_repository_.GetBalance(product_id, warehouse_code);
}

Result<StockBalance> InventoryService::PostMovement(const StockMovement& movement) const {
  if (movement.product_id <= 0) {
    return Result<StockBalance>::Failure(ValidationError("product_id must be greater than zero."));
  }

  const std::string warehouse = Trim(movement.warehouse_code);
  if (warehouse.empty()) {
    return Result<StockBalance>::Failure(ValidationError("warehouse_code must not be empty."));
  }

  if (Trim(movement.reason).empty()) {
    return Result<StockBalance>::Failure(ValidationError("reason must not be empty."));
  }

  if ((movement.movement_type == StockMovementType::Receipt || movement.movement_type == StockMovementType::Issue) &&
      movement.quantity <= 0) {
    return Result<StockBalance>::Failure(
      ValidationError("quantity must be greater than zero for receipt/issue."));
  }

  if (movement.movement_type == StockMovementType::Adjustment && movement.quantity == 0) {
    return Result<StockBalance>::Failure(
      ValidationError("quantity must not be zero for adjustment."));
  }

  auto product_result = product_repository_.GetById(movement.product_id);
  if (!product_result.ok()) {
    return Result<StockBalance>::Failure(product_result.error());
  }
  if (!product_result.value().has_value()) {
    return Result<StockBalance>::Failure(NotFoundError(movement.product_id));
  }
  if (!product_result.value()->is_stock_tracked) {
    return Result<StockBalance>::Failure(
      ValidationError("Stock movement is not allowed for non-stock-tracked product."));
  }

  StockMovement normalized = movement;
  normalized.warehouse_code = warehouse;
  normalized.reason = Trim(movement.reason);

  return inventory_repository_.ApplyMovement(normalized);
}

}  // namespace core
