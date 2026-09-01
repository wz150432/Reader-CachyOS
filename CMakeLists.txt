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
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
    src/app/BookmarkDialog.cpp src/app/BookmarkDialog.h
    src/app/BasicSettingsDialog.cpp src/app/BasicSettingsDialog.h
    src/app/TagsetDialog.cpp src/app/TagsetDialog.h
)
target_include_directories(reader PRIVATE src)
target_link_libraries(reader PRIVATE reader_core Qt6::Widgets)

enable_testing()

add_library(reader_core STATIC
    src/core/TextCodec.cpp src/core/TextCodec.h
    src/core/ChapterParser.cpp src/core/ChapterParser.h
    src/core/Book.cpp src/core/Book.h
    src/core/TextBook.cpp src/core/TextBook.h
    src/core/Page.cpp src/core/Page.h
    src/core/Settings.cpp src/core/Settings.h
    src/core/Cache.cpp src/core/Cache.h
    src/core/Keyset.cpp src/core/Keyset.h
    src/core/NiriConfig.cpp src/core/NiriConfig.h
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

qt_add_executable(tst_settings tests/tst_settings.cpp)
target_link_libraries(tst_settings PRIVATE reader_core Qt6::Test)
add_test(NAME settings COMMAND tst_settings)
set_tests_properties(settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_cache tests/tst_cache.cpp)
target_link_libraries(tst_cache PRIVATE reader_core Qt6::Test)
add_test(NAME cache COMMAND tst_cache)
set_tests_properties(cache PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_readingview
    tests/tst_readingview.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_link_libraries(tst_readingview PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME readingview COMMAND tst_readingview)
set_tests_properties(readingview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_mainwindow
    tests/tst_mainwindow.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
    src/app/BookmarkDialog.cpp src/app/BookmarkDialog.h
    src/app/BasicSettingsDialog.cpp src/app/BasicSettingsDialog.h
    src/app/TagsetDialog.cpp src/app/TagsetDialog.h
)
target_include_directories(tst_mainwindow PRIVATE src)
target_link_libraries(tst_mainwindow PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME mainwindow COMMAND tst_mainwindow)
set_tests_properties(mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_keyset tests/tst_keyset.cpp)
target_link_libraries(tst_keyset PRIVATE reader_core Qt6::Test)
add_test(NAME keyset COMMAND tst_keyset)
set_tests_properties(keyset PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_bookmarks tests/tst_bookmarks.cpp)
target_link_libraries(tst_bookmarks PRIVATE reader_core Qt6::Test)
add_test(NAME bookmarks COMMAND tst_bookmarks)
set_tests_properties(bookmarks PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_page2 tests/tst_page2.cpp)
target_link_libraries(tst_page2 PRIVATE reader_core Qt6::Test)
add_test(NAME page2 COMMAND tst_page2)
set_tests_properties(page2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_settings2 tests/tst_settings2.cpp)
target_link_libraries(tst_settings2 PRIVATE reader_core Qt6::Test)
add_test(NAME settings2 COMMAND tst_settings2)
set_tests_properties(settings2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_readingview2
    tests/tst_readingview2.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(tst_readingview2 PRIVATE src)
target_link_libraries(tst_readingview2 PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME readingview2 COMMAND tst_readingview2)
set_tests_properties(readingview2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_mainwindow2
    tests/tst_mainwindow2.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
    src/app/BookmarkDialog.cpp src/app/BookmarkDialog.h
    src/app/BasicSettingsDialog.cpp src/app/BasicSettingsDialog.h
    src/app/TagsetDialog.cpp src/app/TagsetDialog.h
)
target_include_directories(tst_mainwindow2 PRIVATE src)
target_link_libraries(tst_mainwindow2 PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME mainwindow2 COMMAND tst_mainwindow2)
set_tests_properties(mainwindow2 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_settings3 tests/tst_settings3.cpp)
target_link_libraries(tst_settings3 PRIVATE reader_core Qt6::Test)
add_test(NAME settings3 COMMAND tst_settings3)
set_tests_properties(settings3 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_page3 tests/tst_page3.cpp)
target_link_libraries(tst_page3 PRIVATE reader_core Qt6::Test)
add_test(NAME page3 COMMAND tst_page3)
set_tests_properties(page3 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_autopage
    tests/tst_autopage.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(tst_autopage PRIVATE src)
target_link_libraries(tst_autopage PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME autopage COMMAND tst_autopage)
set_tests_properties(autopage PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_tags
    tests/tst_tags.cpp
    src/app/ReadingView.cpp src/app/ReadingView.h
)
target_include_directories(tst_tags PRIVATE src)
target_link_libraries(tst_tags PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME tags COMMAND tst_tags)
set_tests_properties(tags PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_window
    tests/tst_window.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/ReadingView.cpp src/app/ReadingView.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/KeysetDialog.cpp src/app/KeysetDialog.h
    src/app/BookmarkDialog.cpp src/app/BookmarkDialog.h
    src/app/BasicSettingsDialog.cpp src/app/BasicSettingsDialog.h
    src/app/TagsetDialog.cpp src/app/TagsetDialog.h
)
target_include_directories(tst_window PRIVATE src)
target_link_libraries(tst_window PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_test(NAME window COMMAND tst_window)
set_tests_properties(window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_niriconfig tests/tst_niriconfig.cpp)
target_link_libraries(tst_niriconfig PRIVATE reader_core Qt6::Test)
add_test(NAME niriconfig COMMAND tst_niriconfig)
set_tests_properties(niriconfig PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
