#include <chrono>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "core/inventory.hpp"
#include "core/product.hpp"
#include "infra/sqlite/sqlite_inventory_repository.hpp"
#include "infra/sqlite/sqlite_product_repository.hpp"
#include "test_helpers.hpp"

namespace {

std::string BuildTempDbPath() {
  const auto temp_dir = std::filesystem::temp_directory_path();
  const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  return (temp_dir / ("hybrid_product_inventory_repo_test_" + std::to_string(timestamp) + ".db")).string();
}

}  // namespace

int main() {
  const std::string db_path = BuildTempDbPath();

  {
    infra::sqlite::SQLiteProductRepository product_repo(db_path);
    infra::sqlite::SQLiteInventoryRepository inventory_repo(db_path);

    auto product_migration = product_repo.Migrate();
    TEST_CHECK(product_migration.ok());
    auto inventory_migration = inventory_repo.Migrate();
    TEST_CHECK(inventory_migration.ok());

    core::Product product;
    product.sku = "FG-100";
    product.name = "Console Unit";
    product.category = "Finished";
    product.default_uom = "EA";
    product.product_type = core::ProductType::Finished;
    product.is_stock_tracked = true;
    product.safety_stock = 2;
    product.reorder_point = 5;

    auto created = product_repo.Create(product);
    TEST_CHECK(created.ok());
    TEST_CHECK(created.value().id > 0);

    auto fetched = product_repo.GetById(created.value().id);
    TEST_CHECK(fetched.ok());
    TEST_CHECK(fetched.value().has_value());
    TEST_CHECK(fetched.value()->sku == "FG-100");

    core::ListProductsQuery query;
    query.page = 1;
    query.page_size = 10;
    query.search = "Console";

    auto listed = product_repo.List(query);
    TEST_CHECK(listed.ok());
    TEST_CHECK(listed.value().total == 1);
    TEST_CHECK(listed.value().products.size() == 1);

    auto initial_balance = inventory_repo.GetBalance(created.value().id, "MAIN");
    TEST_CHECK(initial_balance.ok());
    TEST_CHECK(initial_balance.value().on_hand == 0);
    TEST_CHECK(initial_balance.value().below_reorder_point);

    core::StockMovement receipt;
    receipt.product_id = created.value().id;
    receipt.warehouse_code = "MAIN";
    receipt.movement_type = core::StockMovementType::Receipt;
    receipt.quantity = 7;
    receipt.reason = "initial_receipt";

    auto receipt_result = inventory_repo.ApplyMovement(receipt);
    TEST_CHECK(receipt_result.ok());
    TEST_CHECK(receipt_result.value().on_hand == 7);
    TEST_CHECK(!receipt_result.value().below_reorder_point);

    core::StockMovement issue;
    issue.product_id = created.value().id;
    issue.warehouse_code = "MAIN";
    issue.movement_type = core::StockMovementType::Issue;
    issue.quantity = 3;
    issue.reason = "order_allocation";

    auto issue_result = inventory_repo.ApplyMovement(issue);
    TEST_CHECK(issue_result.ok());
    TEST_CHECK(issue_result.value().on_hand == 4);
    TEST_CHECK(issue_result.value().below_reorder_point);

    core::StockMovement invalid_issue = issue;
    invalid_issue.quantity = 40;
    auto invalid_issue_result = inventory_repo.ApplyMovement(invalid_issue);
    TEST_CHECK(!invalid_issue_result.ok());
    TEST_CHECK(invalid_issue_result.error().code == core::ErrorCode::ValidationFailed);

    auto deleted = product_repo.Delete(created.value().id);
    TEST_CHECK(deleted.ok());
    TEST_CHECK(deleted.value());
  }

  std::error_code ec;
  std::filesystem::remove(db_path, ec);

  return 0;
}
