include_directories(PRIVATE "${PROJECT_SOURCE_DIR}/server/include")

add_executable (server "server.cpp" "main.cpp")

target_link_libraries(server PRIVATE ws2_32)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET server PROPERTY CXX_STANDARD 20)
endif()

