#pragma once

#include "core/inventory.hpp"
#include "core/product.hpp"
#include "person.pb.h"

namespace infra::mapping {

hybrid::people::v1::ProductType ToProtoProductType(core::ProductType type);
core::ProductType FromProtoProductType(hybrid::people::v1::ProductType type);

hybrid::people::v1::Product ToProtoProduct(const core::Product& product);
core::Product FromProtoProduct(const hybrid::people::v1::Product& product);
core::Product FromProtoProductInput(const hybrid::people::v1::ProductInput& input);

hybrid::people::v1::StockMovementType ToProtoStockMovementType(core::StockMovementType type);
core::StockMovementType FromProtoStockMovementType(hybrid::people::v1::StockMovementType type);

hybrid::people::v1::StockBalance ToProtoStockBalance(const core::StockBalance& balance);
core::StockMovement FromProtoStockMovementInput(const hybrid::people::v1::StockMovementInput& input);

}  // namespace infra::mapping
