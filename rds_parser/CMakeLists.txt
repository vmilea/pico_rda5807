add_library(rds_parser INTERFACE)

target_include_directories(rds_parser
    INTERFACE
    ./include)

target_sources(rds_parser
    INTERFACE
    rds_parser.c
)
