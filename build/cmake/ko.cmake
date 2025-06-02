function(compile_module obj)
    AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
#    file(COPY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${obj})
    # -G Ninja: USES_TERMINAL只在Ninja有效
    add_custom_target(${obj}
            COMMAND ${CMAKE_COMMAND} -E echo "start compile ${obj} ..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${obj}
            COMMAND sh -c "make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} KERNEL_BUILD_DIR=${KERNEL_BUILD_DIR} SYMVERS_DIR=${SYMVERS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${obj}/Module.symvers ${SYMVERS_DIR}/${obj}.symvers
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${obj}/*.ko ${KO_DIR}/
            COMMAND ${CMAKE_COMMAND} -E echo "end compile ${obj}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${obj}
            USES_TERMINAL
            SOURCES ${SRC_LIST})
endfunction()

function(add_module obj)
    compile_module(${obj})
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} ${obj}_SRC)
    # 只是为了clion提示
    add_executable(${obj}_exe ${${obj}_SRC})
    # 只是为了去除clion的错误提示, 内核编译时会传递宏
    target_compile_definitions(${obj}_exe PRIVATE -DKBUILD_MODNAME=\"modname_${obj}\")
    target_compile_definitions(${obj}_exe PRIVATE -DKBUILD_MODFILE=\"modfile_${obj}\")
    target_compile_definitions(${obj}_exe PRIVATE -DKBUILD_BASENAME=\"basename_${obj}\")
endfunction()