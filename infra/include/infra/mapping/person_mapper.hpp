#pragma once

#include "core/error.hpp"
#include "core/person.hpp"
#include "person.pb.h"

namespace infra::mapping {

hybrid::people::v1::Person ToProtoPerson(const core::Person& person);
core::Person FromProtoPerson(const hybrid::people::v1::Person& person);
core::Person FromProtoPersonInput(const hybrid::people::v1::PersonInput& input);
hybrid::people::v1::ApiError ToProtoApiError(const core::Error& error);

}  // namespace infra::mapping
