# Reader build adapter for pinned upstream sources; no network access at build time.
find_package(ZLIB REQUIRED)
set(mobi_sources buffer compression debug index memory meta parse_rawml read structure util write)
list(TRANSFORM mobi_sources PREPEND "src/")
list(TRANSFORM mobi_sources APPEND ".c")
add_library(reader_mobi SHARED ${mobi_sources})
set_target_properties(reader_mobi PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
    C_STANDARD 99 C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/reader")
target_include_directories(reader_mobi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(reader_mobi PRIVATE
    PACKAGE_VERSION="0.12" HAVE_UNISTD_H HAVE_STRDUP MOBI_INLINE=inline)
target_link_libraries(reader_mobi PRIVATE ZLIB::ZLIB)
