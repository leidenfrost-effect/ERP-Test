#include "infra/mapping/person_mapper.hpp"

namespace infra::mapping {

hybrid::people::v1::Person ToProtoPerson(const core::Person& person) {
  hybrid::people::v1::Person proto;
  proto.set_id(person.id);
  proto.set_first_name(person.first_name);
  proto.set_last_name(person.last_name);
  proto.set_email(person.email);
  proto.set_age(person.age);
  return proto;
}

core::Person FromProtoPerson(const hybrid::people::v1::Person& person) {
  return core::Person{
    person.id(),
    person.first_name(),
    person.last_name(),
    person.email(),
    person.age()
  };
}

core::Person FromProtoPersonInput(const hybrid::people::v1::PersonInput& input) {
  return core::Person{
    0,
    input.first_name(),
    input.last_name(),
    input.email(),
    input.age()
  };
}

hybrid::people::v1::ApiError ToProtoApiError(const core::Error& error) {
  hybrid::people::v1::ApiError proto;
  proto.set_code(core::ToString(error.code));
  proto.set_message(error.message);
  for (const auto& detail : error.details) {
    proto.add_details(detail);
  }
  return proto;
}

}  // namespace infra::mapping
