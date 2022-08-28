add_executable(ckdl-tokenize ckdl-tokenize.c)
target_link_libraries(ckdl-tokenize kdl)

add_executable(ckdl-parse-events ckdl-parse-events.c)
target_link_libraries(ckdl-parse-events kdl)
