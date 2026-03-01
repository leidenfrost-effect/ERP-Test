#pragma once

#include <sqlite3.h>

#include "core/result.hpp"

namespace infra::sqlite {

core::Result<bool> RunMigrations(sqlite3* db);

}  // namespace infra::sqlite
