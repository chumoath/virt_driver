function(compile_module obj)
    AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
    SET(TARGET_NAME ${obj})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    # -G Ninja: USES_TERMINAL只在Ninja有效
    add_custom_target(${TARGET_NAME}
            COMMAND ${CMAKE_COMMAND} -E echo "start compile ${TARGET_NAME} ..."
            COMMAND sh -c "make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} KERNEL_BUILD_DIR=${KERNEL_BUILD_DIR} SYMVERS_DIR=${SYMVERS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${obj}/Module.symvers ${SYMVERS_DIR}/${obj}.symvers
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${obj}/*.ko ${KO_DIR}/
            COMMAND ${CMAKE_COMMAND} -E echo "end compile ${TARGET_NAME}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${obj}
            USES_TERMINAL
            SOURCES ${SRC_LIST})
endfunction()

function(add_module obj)
    compile_module(${obj})
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} ${obj}_SRC)
    # 只是为了clion提示
    add_executable(${obj}_exe ${${obj}_SRC})
    target_compile_definitions(${obj}_exe PRIVATE -DKBUILD_MODNAME=${obj})
endfunction()