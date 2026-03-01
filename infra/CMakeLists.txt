add_library(infra
  src/mapping/product_mapper.cpp
  src/mapping/person_mapper.cpp
  src/sqlite/sqlite_inventory_repository.cpp
  src/sqlite/migrations.cpp
  src/sqlite/sqlite_product_repository.cpp
  src/sqlite/sqlite_person_repository.cpp
)

target_include_directories(infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(infra
  PUBLIC
    core
    person_proto
    protobuf::libprotobuf
)

if (SQLITE_TARGET)
  target_link_libraries(infra PRIVATE ${SQLITE_TARGET})
else()
  target_include_directories(infra PRIVATE ${SQLite3_INCLUDE_DIRS})
  target_link_libraries(infra PRIVATE ${SQLite3_LIBRARIES})
endif()

target_compile_features(infra PUBLIC cxx_std_17)
