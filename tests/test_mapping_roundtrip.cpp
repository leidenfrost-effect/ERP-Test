#include "core/error.hpp"
#include "core/inventory.hpp"
#include "core/person.hpp"
#include "core/product.hpp"
#include "infra/mapping/person_mapper.hpp"
#include "infra/mapping/product_mapper.hpp"
#include "test_helpers.hpp"

int main() {
  core::Person person;
  person.id = 42;
  person.first_name = "Alan";
  person.last_name = "Turing";
  person.email = "alan@example.com";
  person.age = 41;

  const auto proto_person = infra::mapping::ToProtoPerson(person);
  const auto roundtrip_person = infra::mapping::FromProtoPerson(proto_person);

  TEST_CHECK(roundtrip_person.id == person.id);
  TEST_CHECK(roundtrip_person.first_name == person.first_name);
  TEST_CHECK(roundtrip_person.last_name == person.last_name);
  TEST_CHECK(roundtrip_person.email == person.email);
  TEST_CHECK(roundtrip_person.age == person.age);

  hybrid::people::v1::PersonInput input;
  input.set_first_name("Katherine");
  input.set_last_name("Johnson");
  input.set_email("katherine@example.com");
  input.set_age(50);

  const auto mapped_from_input = infra::mapping::FromProtoPersonInput(input);
  TEST_CHECK(mapped_from_input.id == 0);
  TEST_CHECK(mapped_from_input.first_name == "Katherine");
  TEST_CHECK(mapped_from_input.last_name == "Johnson");
  TEST_CHECK(mapped_from_input.email == "katherine@example.com");
  TEST_CHECK(mapped_from_input.age == 50);

  core::Error error{
    core::ErrorCode::ValidationFailed,
    "Validation failed.",
    { "email must be a valid email address." }
  };
  const auto proto_error = infra::mapping::ToProtoApiError(error);
  TEST_CHECK(proto_error.code() == "VALIDATION_FAILED");
  TEST_CHECK(proto_error.message() == "Validation failed.");
  TEST_CHECK(proto_error.details_size() == 1);
  TEST_CHECK(proto_error.details(0) == "email must be a valid email address.");

  core::Product product;
  product.id = 7;
  product.sku = "FG-700";
  product.name = "Control Panel";
  product.category = "Finished";
  product.default_uom = "EA";
  product.product_type = core::ProductType::Finished;
  product.is_stock_tracked = true;
  product.safety_stock = 3;
  product.reorder_point = 9;

  const auto proto_product = infra::mapping::ToProtoProduct(product);
  const auto roundtrip_product = infra::mapping::FromProtoProduct(proto_product);
  TEST_CHECK(roundtrip_product.id == product.id);
  TEST_CHECK(roundtrip_product.sku == product.sku);
  TEST_CHECK(roundtrip_product.name == product.name);
  TEST_CHECK(roundtrip_product.category == product.category);
  TEST_CHECK(roundtrip_product.default_uom == product.default_uom);
  TEST_CHECK(roundtrip_product.product_type == product.product_type);
  TEST_CHECK(roundtrip_product.is_stock_tracked == product.is_stock_tracked);
  TEST_CHECK(roundtrip_product.safety_stock == product.safety_stock);
  TEST_CHECK(roundtrip_product.reorder_point == product.reorder_point);

  hybrid::people::v1::ProductInput product_input;
  product_input.set_sku("RM-010");
  product_input.set_name("Copper Sheet");
  product_input.set_category("Raw");
  product_input.set_default_uom("KG");
  product_input.set_product_type(hybrid::people::v1::PRODUCT_TYPE_RAW);
  product_input.set_is_stock_tracked(true);
  product_input.set_safety_stock(10);
  product_input.set_reorder_point(20);

  const auto mapped_product_input = infra::mapping::FromProtoProductInput(product_input);
  TEST_CHECK(mapped_product_input.id == 0);
  TEST_CHECK(mapped_product_input.sku == "RM-010");
  TEST_CHECK(mapped_product_input.name == "Copper Sheet");
  TEST_CHECK(mapped_product_input.category == "Raw");
  TEST_CHECK(mapped_product_input.default_uom == "KG");
  TEST_CHECK(mapped_product_input.product_type == core::ProductType::Raw);
  TEST_CHECK(mapped_product_input.is_stock_tracked);
  TEST_CHECK(mapped_product_input.safety_stock == 10);
  TEST_CHECK(mapped_product_input.reorder_point == 20);

  core::StockMovement movement;
  movement.product_id = 7;
  movement.warehouse_code = "MAIN";
  movement.movement_type = core::StockMovementType::Issue;
  movement.quantity = 4;
  movement.reason = "job_issue";

  hybrid::people::v1::StockMovementInput movement_input;
  movement_input.set_product_id(movement.product_id);
  movement_input.set_warehouse_code(movement.warehouse_code);
  movement_input.set_movement_type(infra::mapping::ToProtoStockMovementType(movement.movement_type));
  movement_input.set_quantity(movement.quantity);
  movement_input.set_reason(movement.reason);

  const auto mapped_movement = infra::mapping::FromProtoStockMovementInput(movement_input);
  TEST_CHECK(mapped_movement.product_id == movement.product_id);
  TEST_CHECK(mapped_movement.warehouse_code == movement.warehouse_code);
  TEST_CHECK(mapped_movement.movement_type == movement.movement_type);
  TEST_CHECK(mapped_movement.quantity == movement.quantity);
  TEST_CHECK(mapped_movement.reason == movement.reason);

  core::StockBalance balance;
  balance.product_id = 7;
  balance.warehouse_code = "MAIN";
  balance.on_hand = 12;
  balance.reserved = 2;
  balance.available = 10;
  balance.below_reorder_point = false;

  const auto proto_balance = infra::mapping::ToProtoStockBalance(balance);
  TEST_CHECK(proto_balance.product_id() == balance.product_id);
  TEST_CHECK(proto_balance.warehouse_code() == balance.warehouse_code);
  TEST_CHECK(proto_balance.on_hand() == balance.on_hand);
  TEST_CHECK(proto_balance.reserved() == balance.reserved);
  TEST_CHECK(proto_balance.available() == balance.available);
  TEST_CHECK(proto_balance.below_reorder_point() == balance.below_reorder_point);

  return 0;
}
