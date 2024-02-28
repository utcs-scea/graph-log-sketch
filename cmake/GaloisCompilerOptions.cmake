# SPDX-License-Identifier: MIT
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

include_guard()

option(GALOIS_WERROR "Make all warnings into errors." ON)

# Default compiler options for targets
function(galois_compiler_options TARGET)
    set_target_properties(${TARGET}
        PROPERTIES
            CXX_STANDARD                17
            CXX_STANDARD_REQUIRED       ON
            CXX_EXTENSIONS              OFF
            CXX_VISIBILITY_PRESET       hidden
            VISIBILITY_INLINES_HIDDEN   ON
            POSITION_INDEPENDENT_CODE   ON
    )
    target_compile_features(${TARGET}
        PUBLIC
            cxx_std_17)
endfunction()

# Default compiler warnings for targets
function(galois_compiler_warnings TARGET)
    target_compile_options(${TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic $<$<BOOL:${GALOIS_WERROR}>:-Werror>>
    )
endfunction()

function(galois_add_executable TARGET)
    add_executable(${TARGET} ${ARGN})
    #galois_compiler_warnings(${TARGET})
    galois_compiler_options(${TARGET})
endfunction()
