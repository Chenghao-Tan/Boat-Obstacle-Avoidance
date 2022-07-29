# Create library
set(SRC_DIRS
    .
)

set(INCLUDE_DIRS
    c_library_v2
    c_library_v2/ardupilotmega
    .
)

idf_component_register(SRC_DIRS ${SRC_DIRS} INCLUDE_DIRS ${INCLUDE_DIRS})
