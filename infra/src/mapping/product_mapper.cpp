#include "infra/mapping/product_mapper.hpp"

namespace infra::mapping {

hybrid::people::v1::ProductType ToProtoProductType(const core::ProductType type) {
  switch (type) {
    case core::ProductType::Finished:
      return hybrid::people::v1::PRODUCT_TYPE_FINISHED;
    case core::ProductType::Semi:
      return hybrid::people::v1::PRODUCT_TYPE_SEMI;
    case core::ProductType::Raw:
      return hybrid::people::v1::PRODUCT_TYPE_RAW;
  }
  return hybrid::people::v1::PRODUCT_TYPE_UNSPECIFIED;
}

core::ProductType FromProtoProductType(const hybrid::people::v1::ProductType type) {
  switch (type) {
    case hybrid::people::v1::PRODUCT_TYPE_FINISHED:
      return core::ProductType::Finished;
    case hybrid::people::v1::PRODUCT_TYPE_SEMI:
      return core::ProductType::Semi;
    case hybrid::people::v1::PRODUCT_TYPE_RAW:
      return core::ProductType::Raw;
    case hybrid::people::v1::PRODUCT_TYPE_UNSPECIFIED:
      break;
  }
  return core::ProductType::Finished;
}

hybrid::people::v1::Product ToProtoProduct(const core::Product& product) {
  hybrid::people::v1::Product proto;
  proto.set_id(product.id);
  proto.set_sku(product.sku);
  proto.set_name(product.name);
  proto.set_category(product.category);
  proto.set_default_uom(product.default_uom);
  proto.set_product_type(ToProtoProductType(product.product_type));
  proto.set_is_stock_tracked(product.is_stock_tracked);
  proto.set_safety_stock(product.safety_stock);
  proto.set_reorder_point(product.reorder_point);
  return proto;
}

core::Product FromProtoProduct(const hybrid::people::v1::Product& product) {
  return core::Product{
    product.id(),
    product.sku(),
    product.name(),
    product.category(),
    product.default_uom(),
    FromProtoProductType(product.product_type()),
    product.is_stock_tracked(),
    product.safety_stock(),
    product.reorder_point()
  };
}

core::Product FromProtoProductInput(const hybrid::people::v1::ProductInput& input) {
  return core::Product{
    0,
    input.sku(),
    input.name(),
    input.category(),
    input.default_uom(),
    FromProtoProductType(input.product_type()),
    input.is_stock_tracked(),
    input.safety_stock(),
    input.reorder_point()
  };
}

hybrid::people::v1::StockMovementType ToProtoStockMovementType(const core::StockMovementType type) {
  switch (type) {
    case core::StockMovementType::Receipt:
      return hybrid::people::v1::STOCK_MOVEMENT_TYPE_RECEIPT;
    case core::StockMovementType::Issue:
      return hybrid::people::v1::STOCK_MOVEMENT_TYPE_ISSUE;
    case core::StockMovementType::Adjustment:
      return hybrid::people::v1::STOCK_MOVEMENT_TYPE_ADJUSTMENT;
  }
  return hybrid::people::v1::STOCK_MOVEMENT_TYPE_UNSPECIFIED;
}

core::StockMovementType FromProtoStockMovementType(const hybrid::people::v1::StockMovementType type) {
  switch (type) {
    case hybrid::people::v1::STOCK_MOVEMENT_TYPE_RECEIPT:
      return core::StockMovementType::Receipt;
    case hybrid::people::v1::STOCK_MOVEMENT_TYPE_ISSUE:
      return core::StockMovementType::Issue;
    case hybrid::people::v1::STOCK_MOVEMENT_TYPE_ADJUSTMENT:
      return core::StockMovementType::Adjustment;
    case hybrid::people::v1::STOCK_MOVEMENT_TYPE_UNSPECIFIED:
      break;
  }
  return core::StockMovementType::Receipt;
}

hybrid::people::v1::StockBalance ToProtoStockBalance(const core::StockBalance& balance) {
  hybrid::people::v1::StockBalance proto;
  proto.set_product_id(balance.product_id);
  proto.set_warehouse_code(balance.warehouse_code);
  proto.set_on_hand(balance.on_hand);
  proto.set_reserved(balance.reserved);
  proto.set_available(balance.available);
  proto.set_below_reorder_point(balance.below_reorder_point);
  return proto;
}

core::StockMovement FromProtoStockMovementInput(const hybrid::people::v1::StockMovementInput& input) {
  return core::StockMovement{
    input.product_id(),
    input.warehouse_code(),
    FromProtoStockMovementType(input.movement_type()),
    input.quantity(),
    input.reason()
  };
}

}  // namespace infra::mapping
