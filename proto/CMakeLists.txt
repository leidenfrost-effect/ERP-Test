set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/person.proto)

add_library(person_proto ${PROTO_FILE})
target_link_libraries(person_proto PUBLIC protobuf::libprotobuf)
target_include_directories(person_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate(
  TARGET person_proto
  LANGUAGE cpp
  PROTOS ${PROTO_FILE}
  IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
