cmake_minimum_required(VERSION 3.20)
project(BounceChecker CXX)

# Path to a checkout of https://github.com/bakkesmodorg/BakkesModSDK
# (BakkesMod also ships it at %APPDATA%/bakkesmod/bakkesmod/bakkesmodsdk)
set(BAKKESMOD_SDK "$ENV{APPDATA}/bakkesmod/bakkesmod/bakkesmodsdk" CACHE PATH "BakkesMod SDK folder")

if(NOT MSVC)
  message(FATAL_ERROR "BakkesMod plugins must be built with MSVC (x64).")
endif()

add_library(BounceChecker SHARED src/BounceChecker.cpp)
set_target_properties(BounceChecker PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(BounceChecker PRIVATE src "${BAKKESMOD_SDK}/include")
target_link_libraries(BounceChecker PRIVATE "${BAKKESMOD_SDK}/lib/pluginsdk.lib")
target_compile_definitions(BounceChecker PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES)
target_compile_options(BounceChecker PRIVATE /W3 /permissive- /utf-8)
