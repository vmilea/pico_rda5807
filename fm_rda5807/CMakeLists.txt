add_library(fm_rda5807 INTERFACE)

target_include_directories(fm_rda5807
    INTERFACE
    ./include)

target_sources(fm_rda5807
    INTERFACE
    fm_rda5807.c
)

target_link_libraries(fm_rda5807
    INTERFACE
    hardware_i2c
)
