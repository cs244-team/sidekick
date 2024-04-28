file(GLOB SOURCES *.cc *.hh)

add_library(util STATIC ${SOURCES})

target_include_directories(util PUBLIC .)