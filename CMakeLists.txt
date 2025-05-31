cmake_minimum_required(VERSION 3.2)

project(infected)

set(SYMVERS_DIR ${CMAKE_BINARY_DIR}/symvers)
set(KO_DIR ${CMAKE_BINARY_DIR}/ko)

file(MAKE_DIRECTORY ${SYMVERS_DIR})
file(MAKE_DIRECTORY ${KO_DIR})
#file(COPY ${CMAKE_SOURCE_DIR}/vmlinux.symvers DESTINATION ${SYMVERS_DIR})

include(build/cmake/ko.cmake)

message(STATUS "KERNEL_DIR=${KERNEL_DIR}")
message(STATUS "KERNEL_BUILD_DIR=${KERNEL_BUILD_DIR}")

include_directories(${KERNEL_DIR}/arch/arm/include)
include_directories(${KERNEL_DIR}/include)
include_directories(${KERNEL_DIR}/arch/arm/include/uapi)
include_directories(${KERNEL_DIR}/include/uapi)
include_directories(${KERNEL_BUILD_DIR}/arch/arm/include/generated)
include_directories(${KERNEL_BUILD_DIR}/arch/arm/include/generated/uapi)
include_directories(${KERNEL_BUILD_DIR}/include/generated/uapi)
include_directories(${KERNEL_BUILD_DIR}/include)

add_compile_options(-include ${KERNEL_DIR}/include/linux/compiler-version.h)
add_compile_options(-include ${KERNEL_DIR}/include/linux/kconfig.h)
add_compile_options(-include ${KERNEL_DIR}/include/linux/compiler_types.h)

add_definitions(-D__KERNEL__)
add_definitions(-D__LINUX_ARM_ARCH__=7)

set (ko_targets virt_temp)

add_subdirectory(infected)

add_custom_target(infected ALL)
add_dependencies(infected ${ko_targets})
