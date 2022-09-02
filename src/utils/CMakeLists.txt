add_executable(ckdl-tokenize ckdl-tokenize.c)
target_link_libraries(ckdl-tokenize kdl)

add_executable(ckdl-parse-events ckdl-parse-events.c)
target_link_libraries(ckdl-parse-events kdl)

add_library(ckdl-cat STATIC ckdl-cat.c)
target_link_libraries(ckdl-cat PUBLIC kdl)

add_executable(ckdl-cat-app ckdl-cat-main.c)
target_link_libraries(ckdl-cat-app ckdl-cat)
set_target_properties(ckdl-cat-app PROPERTIES OUTPUT_NAME ckdl-cat)
