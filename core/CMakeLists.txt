add_library(core
  src/inventory_service.cpp
  src/product_service.cpp
  src/product_validator.cpp
  src/person_service.cpp
  src/person_validator.cpp
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(core PUBLIC cxx_std_17)
