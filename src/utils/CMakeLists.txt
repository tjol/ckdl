add_executable(ckdl-tokenize ckdl-tokenize.c)
target_link_libraries(ckdl-tokenize kdl)

add_executable(ckdl-parse-events ckdl-parse-events.c)
target_link_libraries(ckdl-parse-events kdl)

add_library(ckdl-cat STATIC ckdl-cat.c)
target_link_libraries(ckdl-cat PUBLIC kdl)
if(WIN32)
    set_target_properties(ckdl-cat PROPERTIES OUTPUT_NAME ckdl-cat-lib)
endif()
target_include_directories(ckdl-cat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ckdl-cat-app ckdl-cat-main.c)
target_link_libraries(ckdl-cat-app ckdl-cat)
set_target_properties(ckdl-cat-app PROPERTIES OUTPUT_NAME ckdl-cat)

include(GNUInstallDirs)
install(TARGETS ckdl-cat-app ckdl-parse-events ckdl-tokenize DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ckdl-utils EXCLUDE_FROM_ALL)
