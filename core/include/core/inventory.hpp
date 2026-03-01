#pragma once

#include <cstdint>
#include <string>

namespace core {

enum class StockMovementType {
  Receipt,
  Issue,
  Adjustment
};

struct StockBalance {
  std::int64_t product_id{0};
  std::string warehouse_code;
  std::int64_t on_hand{0};
  std::int64_t reserved{0};
  std::int64_t available{0};
  bool below_reorder_point{false};
};

struct StockMovement {
  std::int64_t product_id{0};
  std::string warehouse_code;
  StockMovementType movement_type{StockMovementType::Receipt};
  std::int64_t quantity{0};
  std::string reason;
};

}  // namespace core
