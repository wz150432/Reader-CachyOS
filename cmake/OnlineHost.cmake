target_sources(reader_core PRIVATE
 src/core/online/BookSource.cpp src/core/online/BookSourceStore.cpp
 src/core/online/OnlineBook.cpp src/core/online/OnlineEngineBridge.cpp
 src/core/online/OnlineReadingController.cpp)
qt_add_executable(tst_onlinehost tests/tst_onlinehost.cpp)
target_link_libraries(tst_onlinehost PRIVATE reader_core Qt6::Test)
add_test(NAME onlinehost COMMAND tst_onlinehost)
qt_add_executable(online_fixture tests/online_fixture.cpp)
target_link_libraries(online_fixture PRIVATE Qt6::Core)
qt_add_executable(tst_onlinereading tests/tst_onlinereading.cpp src/app/ReadingView.cpp src/app/ReadingView.h)
target_link_libraries(tst_onlinereading PRIVATE reader_core Qt6::Test Qt6::Widgets)
add_dependencies(tst_onlinereading online_fixture)
add_test(NAME onlinereading COMMAND tst_onlinereading)
set_tests_properties(onlinehost onlinereading PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=none")
