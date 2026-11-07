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

# The build number is the commit count, which the release tags carry and the update check compares against. Docker and CI pass it in,
# since a container build has no .git.
if(NOT DEFINED DW_BUILD_NUMBER)
    execute_process(COMMAND git rev-list --count HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE DW_BUILD_NUMBER OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE DW_BUILD_NUMBER_RESULT)
    if(NOT DW_BUILD_NUMBER_RESULT EQUAL 0)
        set(DW_BUILD_NUMBER 0)
    endif()
endif()
message(STATUS "Build number: ${DW_BUILD_NUMBER}")

add_executable(discord_webview src/main.cpp src/UpdateCheck.cpp ${DW_HOST})
target_link_libraries(discord_webview PRIVATE webview_window)
target_compile_definitions(discord_webview PRIVATE DW_BUILD_NUMBER=${DW_BUILD_NUMBER})

# The executable goes to bin/<platform>/<config>, the same layout as gwdevhub_gwlauncher.
set_target_properties(discord_webview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${DW_PLATFORM}/$<LOWER_CASE:$<CONFIG>>")

if(WIN32)
    target_link_options(discord_webview PRIVATE -static -mwindows)
    # The program carries WebView2Loader.dll, which the library writes beside the executable at start: the release is the executable alone.
    webview_window_embed_loader(discord_webview)
endif()
