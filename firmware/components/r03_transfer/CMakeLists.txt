# Pure C, no ESP-IDF dependency: also compiled by test/host for unit tests.
idf_component_register(SRCS "r03.c"
                       INCLUDE_DIRS "include")
