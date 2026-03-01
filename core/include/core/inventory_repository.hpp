#pragma once

#include <cstdint>
#include <string>

#include "core/inventory.hpp"
#include "core/result.hpp"

namespace core {

class IInventoryRepository {
 public:
  virtual ~IInventoryRepository() = default;

  virtual Result<StockBalance> GetBalance(std::int64_t product_id, const std::string& warehouse_code) const = 0;
  virtual Result<StockBalance> ApplyMovement(const StockMovement& movement) = 0;
};

}  // namespace core
