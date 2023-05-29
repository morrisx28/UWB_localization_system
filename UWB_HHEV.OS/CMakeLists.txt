
cmake_minimum_required(VERSION 3.16)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

if(NOT DEFINED ACRT_SDK_ROOT)
    set(ACRT_SDK_ROOT "/opt/autocore")
endif()
set(ACRT_DEP_ROOT "${ACRT_SDK_ROOT}/../autocore-deps")

execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE HOST_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(ACRT_SDK_LIBDIR ${ACRT_SDK_ROOT}/lib)
set(ACRT_DEP_LIBDIR ${ACRT_DEP_ROOT}/lib)
if (DEFINED BUILD_ARCH AND DEFINED HOST_ARCH AND (NOT ${BUILD_ARCH} STREQUAL ${HOST_ARCH}))
    if (${BUILD_ARCH} STREQUAL "aarch64")
        set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc-9)
        set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++-9)
        set(CMAKE_LD aarch64-linux-gnu-ld)
        set(CMAKE_AR aarch64-linux-gnu-ar)
        set(CMAKE_AS aarch64-linux-gnu-as)
        set(CMAKE_NM aarch64-linux-gnu-nm)
        set(CMAKE_STRIP aarch64-linux-gnu-strip)

        set(ACRT_SDK_LIBDIR ${ACRT_SDK_ROOT}/libaarch64)
        set(ACRT_DEP_LIBDIR ${ACRT_DEP_ROOT}/libaarch64)
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${ACRT_SDK_LIBDIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${ACRT_SDK_LIBDIR}/cmake)


set(RUNNABLE_NAME "RunnableUWB")

project(${RUNNABLE_NAME}Runnable)



find_package(AutoCoreDDS REQUIRED)
include("cmcg/cmcg")


cmcg_generate(
    TARGET ${RUNNABLE_NAME}_cmcg
    GEN_IDL
    FILES services/UWB.json

)



set(RUNNABLE_LIB ${RUNNABLE_NAME})
add_library(${RUNNABLE_LIB} SHARED)
set_target_properties(${RUNNABLE_LIB} PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(${RUNNABLE_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(${RUNNABLE_LIB} INTERFACE ${CXX_COMPILE_FLAGS})
target_sources(${RUNNABLE_LIB}
    PRIVATE ${RUNNABLE_NAME}.cc
    PRIVATE TinyJson.cc
    PRIVATE serial_port.cpp
    PRIVATE UWB_manager.cc
)

target_include_directories(${RUNNABLE_LIB}
    PRIVATE ${ACRT_SDK_ROOT}/include
    PRIVATE include
)

target_link_directories(${RUNNABLE_LIB}
    PRIVATE ${ACRT_SDK_LIBDIR}
    PRIVATE ${ACRT_DEP_LIBDIR}
)

target_link_libraries(${RUNNABLE_LIB}

    PRIVATE ${RUNNABLE_NAME}_cmcg

    PUBLIC acfwcm
    PUBLIC acfwexecute
)

install(
    TARGETS ${RUNNABLE_LIB}
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/dist/lib COMPONENT lib
)

set(LOADER_EXEC ${RUNNABLE_NAME}Loader)
add_executable(${LOADER_EXEC})
target_sources(${LOADER_EXEC}
    PRIVATE Loader.cc
)
target_include_directories(${LOADER_EXEC}
    PRIVATE ${ACRT_SDK_ROOT}/include
)

target_link_directories(${LOADER_EXEC}
    PRIVATE ${ACRT_SDK_LIBDIR}
)

target_link_libraries(${LOADER_EXEC}
    PRIVATE acfwruntime
)

install(
    TARGETS ${LOADER_EXEC}
    DESTINATION ${CMAKE_BINARY_DIR}/dist/bin COMPONENT dev
)
