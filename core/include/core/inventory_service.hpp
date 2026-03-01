#pragma once

#include <cstdint>
#include <string>

#include "core/inventory_repository.hpp"
#include "core/product_repository.hpp"
#include "core/result.hpp"

namespace core {

class InventoryService {
 public:
  InventoryService(IInventoryRepository& inventory_repository, IProductRepository& product_repository);

  Result<StockBalance> GetStockBalance(std::int64_t product_id, std::string warehouse_code) const;
  Result<StockBalance> PostMovement(const StockMovement& movement) const;

 private:
  IInventoryRepository& inventory_repository_;
  IProductRepository& product_repository_;
};

}  // namespace core
