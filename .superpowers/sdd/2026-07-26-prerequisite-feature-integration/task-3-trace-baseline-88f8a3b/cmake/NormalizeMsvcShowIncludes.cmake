set(_arpg_msvc_showincludes_mojibake "娉ㄦ剰: 鍖呭惈鏂囦欢:  ")
set(_arpg_msvc_showincludes_utf8 "注意: 包含文件:  ")

if(MSVC AND CMAKE_GENERATOR MATCHES "^Ninja")
    foreach(_arpg_prefix_variable IN ITEMS
            CMAKE_C_CL_SHOWINCLUDES_PREFIX
            CMAKE_CXX_CL_SHOWINCLUDES_PREFIX
            CMAKE_CL_SHOWINCLUDES_PREFIX)
        if(DEFINED ${_arpg_prefix_variable}
                AND "${${_arpg_prefix_variable}}" STREQUAL
                    "${_arpg_msvc_showincludes_mojibake}")
            set(${_arpg_prefix_variable} "${_arpg_msvc_showincludes_utf8}")
        endif()
    endforeach()
endif()

unset(_arpg_prefix_variable)
unset(_arpg_msvc_showincludes_mojibake)
unset(_arpg_msvc_showincludes_utf8)
