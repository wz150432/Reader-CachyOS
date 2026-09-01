cmake_minimum_required(VERSION 3.21)
project(ReaderCachyOS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

qt_add_executable(reader
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
)
target_include_directories(reader PRIVATE src)
target_link_libraries(reader PRIVATE Qt6::Widgets)

enable_testing()

add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h
    src/core/Page.cpp src/core/Page.h
)
target_include_directories(reader_core PUBLIC src)
target_link_libraries(reader_core PUBLIC Qt6::Core Qt6::Gui)

qt_add_executable(tst_textcodec tests/tst_textcodec.cpp)
target_link_libraries(tst_textcodec PRIVATE reader_core Qt6::Test)
add_test(NAME textcodec COMMAND tst_textcodec)
set_tests_properties(textcodec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_chapterparser tests/tst_chapterparser.cpp)
target_link_libraries(tst_chapterparser PRIVATE reader_core Qt6::Test)
add_test(NAME chapterparser COMMAND tst_chapterparser)
set_tests_properties(chapterparser PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_textbook tests/tst_textbook.cpp)
target_link_libraries(tst_textbook PRIVATE reader_core Qt6::Test)
add_test(NAME textbook COMMAND tst_textbook)
set_tests_properties(textbook PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_page tests/tst_page.cpp)
target_link_libraries(tst_page PRIVATE reader_core Qt6::Test)
add_test(NAME page COMMAND tst_page)
set_tests_properties(page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
