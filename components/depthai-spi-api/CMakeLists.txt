# Create library
set(COMPONENT_SRCDIRS
    depthai-spi-library
    common
    depthai-shared/src/datatype
    .
)

set(COMPONENT_ADD_INCLUDEDIRS
    depthai-shared/include
    depthai-spi-library
    common
    .
)

set(COMPONENT_REQUIRES "nvs_flash")

register_component()

