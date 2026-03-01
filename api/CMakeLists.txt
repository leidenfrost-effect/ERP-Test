add_executable(person_api
  src/main.cpp
)

target_link_libraries(person_api
  PRIVATE
    core
    infra
    person_proto
    protobuf::libprotobuf
    ${CROW_TARGET}
)

target_compile_features(person_api PRIVATE cxx_std_17)
