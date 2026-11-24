cmake_minimum_required(VERSION 3.20)
project(RLBounceLauncher CXX)

add_executable(RLBounceLauncher main.cpp)
set_target_properties(RLBounceLauncher PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(RLBounceLauncher PRIVATE UNICODE _UNICODE)
target_link_libraries(RLBounceLauncher PRIVATE shell32 ole32 uuid advapi32)
if(MSVC)
  set_property(TARGET RLBounceLauncher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT, no VC++ redist needed
else()
  target_link_options(RLBounceLauncher PRIVATE -municode -static)
endif()
