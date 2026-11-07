cmake_minimum_required(VERSION 3.20)
project(DiscordWebView CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WebViewWindow is a sibling checkout by default. Point this at another checkout if it lives elsewhere.
set(WEBVIEW_WINDOW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WebViewWindow" CACHE PATH "Path to the WebViewWindow checkout")

set(WEBVIEW_WINDOW_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory("${WEBVIEW_WINDOW_DIR}" WebViewWindow)

# The host file defines the desktop functions for the platform being built.
if(WIN32)
    set(DW_PLATFORM windows)
    set(DW_HOST src/Host.windows.cpp)
else()
    set(DW_PLATFORM linux)
    set(DW_HOST src/Host.linux.cpp)
endif()

add_executable(discord_webview src/main.cpp ${DW_HOST})
target_link_libraries(discord_webview PRIVATE webview_window)

# The executable goes to bin/<platform>/<config>, the same layout as gwdevhub_gwlauncher.
set_target_properties(discord_webview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${DW_PLATFORM}/$<LOWER_CASE:$<CONFIG>>")

if(WIN32)
    target_link_options(discord_webview PRIVATE -static -mwindows)
    # WebView2 is loaded at runtime from WebView2Loader.dll, which must sit beside the executable.
    add_custom_command(TARGET discord_webview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEBVIEW_WINDOW_DIR}/third_party/webview2/bin/x64/WebView2Loader.dll" "$<TARGET_FILE_DIR:discord_webview>"
        VERBATIM)
endif()
