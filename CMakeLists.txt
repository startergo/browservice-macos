cmake_minimum_required(VERSION 3.14)
project(Browservice)

set(CMAKE_CXX_STANDARD 17)

# Detect macOS
if(APPLE)
    message(STATUS "Configuring for macOS")
    add_compile_definitions(USE_STD_CEF)
    
    # Add Homebrew paths (Apple Silicon)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    include_directories("/opt/homebrew/include")
    link_directories("/opt/homebrew/lib")
    
    find_package(PkgConfig REQUIRED)
    
    # Use Homebrew pkg-config path + local fix for bzip2
    set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/local_pkgconfig:/opt/homebrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    
    pkg_check_modules(PANGO REQUIRED pango pangoft2)
    # pkg_check_modules(FREETYPE REQUIRED freetype2) - fails on bzip2 dependency
    
    find_package(Freetype REQUIRED)
    
    include_directories(${PANGO_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
    link_directories(${PANGO_LIBRARY_DIRS})
    
    set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
    add_subdirectory(${CEF_ROOT} libcef_dll_wrapper)
    
    # Ensure libcef_dll_wrapper propagates the Framework dependency
    if(APPLE)
        target_link_libraries(libcef_dll_wrapper PUBLIC "${CEF_ROOT}/Release/Chromium Embedded Framework.framework/Chromium Embedded Framework")
    endif()
else()
    message(FATAL_ERROR "This CMakeLists.txt is currently only configured for macOS. Use Makefile for Linux.")
endif()

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Exclude Linux-specific files on macOS
if(APPLE)
    list(FILTER SOURCES EXCLUDE REGEX ".*xvfb.*")
endif()

# Executable
add_executable(browservice ${SOURCES})

# Include directories
target_include_directories(browservice PRIVATE src ${CEF_ROOT})

# OpenSSL (if needed, or use system SSL)
# Browservice uses Poco which might need SSL

# Link libraries
target_link_libraries(browservice PRIVATE libcef_dll_wrapper)

# Link against Poco and other dependencies (assuming they are found in standard paths or HB)
target_link_libraries(browservice PRIVATE
    PocoFoundation PocoNet PocoUtil PocoJSON
    jpeg turbojpeg
    ${PANGO_LIBRARIES} ${FREETYPE_LIBRARIES}
    intl
)

if(APPLE)
    # Add the Framework to the target link libraries
    target_link_libraries(browservice PRIVATE "${CEF_ROOT}/Release/Chromium Embedded Framework.framework/Chromium Embedded Framework")
    
    # Also link standard libs defined by CEF (AppKit, etc)
    target_link_libraries(browservice PRIVATE ${CEF_STANDARD_LIBS})
endif()

# Copy helper app bundle (CEF requirement on macOS)
add_custom_command(TARGET browservice POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CEF_ROOT}/Release/Chromium Embedded Framework.framework"
    "$<TARGET_FILE_DIR:browservice>/Chromium Embedded Framework.framework"
)

# Helper for creating app bundle structure (simplified for CLI tool)

add_subdirectory(viceplugins/retrojsvice)

if(APPLE)
    # Copy retrojsvice.so to the app bundle
    # Location - browservice.app/Contents/PlugIns/retrojsvice.so
    
    add_custom_command(TARGET browservice POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:browservice>/../PlugIns"
        COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:retrojsvice>"
        "$<TARGET_FILE_DIR:browservice>/../PlugIns/$<TARGET_FILE_NAME:retrojsvice>"
    )
endif()

