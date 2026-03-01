#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/inventory_service.hpp"
#include "core/product_service.hpp"
#include "test_helpers.hpp"

namespace {

struct KeyHash {
  std::size_t operator()(const std::pair<std::int64_t, std::string>& key) const {
    return std::hash<std::int64_t>{}(key.first) ^ (std::hash<std::string>{}(key.second) << 1U);
  }
};

class InMemoryProductRepository : public core::IProductRepository {
 public:
  core::Result<core::ListProductsResult> List(const core::ListProductsQuery& query) const override {
    const std::int32_t page = std::max<std::int32_t>(1, query.page);
    const std::int32_t page_size = std::clamp<std::int32_t>(query.page_size, 1, 100);

    std::vector<core::Product> filtered = products_;
    if (!query.search.empty()) {
      filtered.erase(
        std::remove_if(filtered.begin(), filtered.end(), [&](const core::Product& product) {
          return product.sku.find(query.search) == std::string::npos &&
                 product.name.find(query.search) == std::string::npos &&
                 product.category.find(query.search) == std::string::npos;
        }),
        filtered.end());
    }

    const std::int64_t total = static_cast<std::int64_t>(filtered.size());
    const std::size_t offset = static_cast<std::size_t>((page - 1) * page_size);
    const std::size_t end =
      std::min<std::size_t>(filtered.size(), offset + static_cast<std::size_t>(page_size));

    std::vector<core::Product> page_items;
    if (offset < filtered.size()) {
      page_items.assign(filtered.begin() + static_cast<std::ptrdiff_t>(offset),
                        filtered.begin() + static_cast<std::ptrdiff_t>(end));
    }

    core::ListProductsResult result;
    result.products = std::move(page_items);
    result.total = total;
    result.page = page;
    result.page_size = page_size;
    result.search = query.search;
    return core::Result<core::ListProductsResult>::Success(std::move(result));
  }

  core::Result<std::optional<core::Product>> GetById(const std::int64_t id) const override {
    const auto it = std::find_if(products_.begin(), products_.end(), [id](const core::Product& product) {
      return product.id == id;
    });
    if (it == products_.end()) {
      return core::Result<std::optional<core::Product>>::Success(std::nullopt);
    }
    return core::Result<std::optional<core::Product>>::Success(*it);
  }

  core::Result<core::Product> Create(const core::Product& product) override {
    core::Product created = product;
    created.id = next_id_++;
    products_.push_back(created);
    return core::Result<core::Product>::Success(created);
  }

  core::Result<bool> Update(const std::int64_t id, const core::Product& product) override {
    const auto it = std::find_if(products_.begin(), products_.end(), [id](const core::Product& current) {
      return current.id == id;
    });
    if (it == products_.end()) {
      return core::Result<bool>::Success(false);
    }

    core::Product updated = product;
    updated.id = id;
    *it = updated;
    return core::Result<bool>::Success(true);
  }

  core::Result<bool> Delete(const std::int64_t id) override {
    const auto old_size = products_.size();
    products_.erase(
      std::remove_if(products_.begin(), products_.end(), [id](const core::Product& product) {
        return product.id == id;
      }),
      products_.end());
    return core::Result<bool>::Success(products_.size() != old_size);
  }

 private:
  std::vector<core::Product> products_;
  std::int64_t next_id_{1};
};

class InMemoryInventoryRepository : public core::IInventoryRepository {
 public:
  core::Result<core::StockBalance> GetBalance(
    const std::int64_t product_id,
    const std::string& warehouse_code) const override {
    const auto key = std::make_pair(product_id, warehouse_code);
    const auto it = balances_.find(key);
    if (it == balances_.end()) {
      core::StockBalance empty;
      empty.product_id = product_id;
      empty.warehouse_code = warehouse_code;
      return core::Result<core::StockBalance>::Success(empty);
    }
    return core::Result<core::StockBalance>::Success(it->second);
  }

  core::Result<core::StockBalance> ApplyMovement(const core::StockMovement& movement) override {
    const auto key = std::make_pair(movement.product_id, movement.warehouse_code);
    auto current = GetBalance(movement.product_id, movement.warehouse_code).value();

    std::int64_t delta = movement.quantity;
    if (movement.movement_type == core::StockMovementType::Issue) {
      delta = -movement.quantity;
    }
    const auto new_on_hand = current.on_hand + delta;
    if (new_on_hand < 0) {
      return core::Result<core::StockBalance>::Failure(core::Error{
        core::ErrorCode::ValidationFailed,
        "Stock movement would result in negative on_hand.",
        {}
      });
    }

    current.on_hand = new_on_hand;
    current.available = current.on_hand - current.reserved;
    current.below_reorder_point = false;
    balances_[key] = current;
    return core::Result<core::StockBalance>::Success(current);
  }

 private:
  std::unordered_map<std::pair<std::int64_t, std::string>, core::StockBalance, KeyHash> balances_;
};

}  // namespace

int main() {
  InMemoryProductRepository product_repo;
  InMemoryInventoryRepository inventory_repo;
  core::ProductService product_service(product_repo);
  core::InventoryService inventory_service(inventory_repo, product_repo);

  core::Product invalid_product;
  invalid_product.sku = "x";
  invalid_product.name = "";
  invalid_product.category = "";
  invalid_product.default_uom = "";

  auto invalid_create = product_service.CreateProduct(invalid_product);
  TEST_CHECK(!invalid_create.ok());
  TEST_CHECK(invalid_create.error().code == core::ErrorCode::ValidationFailed);

  core::Product tracked_product;
  tracked_product.sku = "FG-001";
  tracked_product.name = "Widget";
  tracked_product.category = "Finished";
  tracked_product.default_uom = "EA";
  tracked_product.product_type = core::ProductType::Finished;
  tracked_product.is_stock_tracked = true;
  tracked_product.safety_stock = 2;
  tracked_product.reorder_point = 5;

  auto created = product_service.CreateProduct(tracked_product);
  TEST_CHECK(created.ok());
  TEST_CHECK(created.value().id > 0);

  auto listed = product_service.ListProducts(1, 10, "FG-");
  TEST_CHECK(listed.ok());
  TEST_CHECK(listed.value().total == 1);

  auto initial_balance = inventory_service.GetStockBalance(created.value().id, "MAIN");
  TEST_CHECK(initial_balance.ok());
  TEST_CHECK(initial_balance.value().on_hand == 0);

  core::StockMovement receipt;
  receipt.product_id = created.value().id;
  receipt.warehouse_code = "MAIN";
  receipt.movement_type = core::StockMovementType::Receipt;
  receipt.quantity = 10;
  receipt.reason = "initial_receipt";

  auto receipt_result = inventory_service.PostMovement(receipt);
  TEST_CHECK(receipt_result.ok());
  TEST_CHECK(receipt_result.value().on_hand == 10);

  core::StockMovement issue;
  issue.product_id = created.value().id;
  issue.warehouse_code = "MAIN";
  issue.movement_type = core::StockMovementType::Issue;
  issue.quantity = 4;
  issue.reason = "production_issue";

  auto issue_result = inventory_service.PostMovement(issue);
  TEST_CHECK(issue_result.ok());
  TEST_CHECK(issue_result.value().on_hand == 6);

  core::StockMovement over_issue = issue;
  over_issue.quantity = 100;
  auto over_issue_result = inventory_service.PostMovement(over_issue);
  TEST_CHECK(!over_issue_result.ok());
  TEST_CHECK(over_issue_result.error().code == core::ErrorCode::ValidationFailed);

  core::Product service_product;
  service_product.sku = "SV-001";
  service_product.name = "Implementation Service";
  service_product.category = "Service";
  service_product.default_uom = "HR";
  service_product.product_type = core::ProductType::Semi;
  service_product.is_stock_tracked = false;

  auto service_created = product_service.CreateProduct(service_product);
  TEST_CHECK(service_created.ok());

  core::StockMovement service_movement;
  service_movement.product_id = service_created.value().id;
  service_movement.warehouse_code = "MAIN";
  service_movement.movement_type = core::StockMovementType::Receipt;
  service_movement.quantity = 1;
  service_movement.reason = "invalid_for_non_stock";

  auto service_move_result = inventory_service.PostMovement(service_movement);
  TEST_CHECK(!service_move_result.ok());
  TEST_CHECK(service_move_result.error().code == core::ErrorCode::ValidationFailed);

  return 0;
}
