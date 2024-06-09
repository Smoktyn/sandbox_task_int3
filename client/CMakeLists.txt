include_directories(PRIVATE "${PROJECT_SOURCE_DIR}/client/include")

add_executable (client "main.cpp" "client.cpp")

target_link_libraries(client PRIVATE ws2_32)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET client PROPERTY CXX_STANDARD 20)
endif()

