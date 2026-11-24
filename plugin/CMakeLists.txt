cmake_minimum_required(VERSION 3.20)
project(BounceChecker CXX)

# Path to a checkout of https://github.com/bakkesmodorg/BakkesModSDK
# (BakkesMod also ships it at %APPDATA%/bakkesmod/bakkesmod/bakkesmodsdk)
set(BAKKESMOD_SDK "$ENV{APPDATA}/bakkesmod/bakkesmod/bakkesmodsdk" CACHE PATH "BakkesMod SDK folder")

if(NOT MSVC)
  message(FATAL_ERROR "BakkesMod plugins must be built with MSVC (x64).")
endif()

# BakkesMod's ImGui build ships zipped inside the SDK - unpack it once into the build folder.
set(IMGUI_ROOT "${CMAKE_BINARY_DIR}/imgui_bm")
if(NOT EXISTS "${IMGUI_ROOT}/imgui/imgui.h")
  file(MAKE_DIRECTORY "${IMGUI_ROOT}")
  file(ARCHIVE_EXTRACT INPUT "${BAKKESMOD_SDK}/imgui_bm.zip" DESTINATION "${IMGUI_ROOT}")
endif()

add_library(BounceChecker SHARED
  src/BounceChecker.cpp
  "${IMGUI_ROOT}/imgui/imgui.cpp"
  "${IMGUI_ROOT}/imgui/imgui_draw.cpp"
  "${IMGUI_ROOT}/imgui/imgui_widgets.cpp"
  "${IMGUI_ROOT}/imgui/imgui_demo.cpp"
)
set_target_properties(BounceChecker PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(BounceChecker PRIVATE src "${BAKKESMOD_SDK}/include" "${IMGUI_ROOT}" "${IMGUI_ROOT}/imgui")
target_link_libraries(BounceChecker PRIVATE "${BAKKESMOD_SDK}/lib/pluginsdk.lib")
target_compile_definitions(BounceChecker PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES)
target_compile_options(BounceChecker PRIVATE /W3 /utf-8)
