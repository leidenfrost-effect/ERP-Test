#pragma once

#include <cstdint>
#include <string>

namespace core {

struct Person {
  std::int64_t id{0};
  std::string first_name;
  std::string last_name;
  std::string email;
  std::int32_t age{0};
};

}  // namespace core
