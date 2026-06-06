cmake_minimum_required(VERSION 3.20)

project(HIMSSoftware VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

find_package(ftxui CONFIG QUIET)
if(NOT ftxui_FOUND)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
  )
  FetchContent_MakeAvailable(ftxui)
endif()

add_executable(hims
  src/main.cpp
  src/app.cpp
  src/ui/shared/AppUiShared.cpp
  src/ui/pages/DashboardPage.cpp
  src/ui/pages/StockPage.cpp
  src/ui/pages/DetailPage.cpp
  src/core/Inventory.cpp
  src/platform/HttpServer.cpp
  src/platform/DigiKeyApi.cpp
  src/platform/Console.cpp
)

target_include_directories(hims PRIVATE src)

if(WIN32)
  target_link_libraries(hims PRIVATE ws2_32 shell32 winhttp)
endif()

target_link_libraries(hims PRIVATE ftxui::screen ftxui::dom ftxui::component)

add_custom_command(TARGET hims POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/scanner.html"
          "$<TARGET_FILE_DIR:hims>/scanner.html"
)

add_executable(hims_tests
  tests/hims_tests.cpp
  src/core/Inventory.cpp
)

target_include_directories(hims_tests PRIVATE src)
