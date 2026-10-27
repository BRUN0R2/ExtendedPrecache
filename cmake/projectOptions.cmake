function(extendedPrecacheApplyOptions targetName)
    set_target_properties(${targetName} PROPERTIES
        CXX_STANDARD 26
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    if(MSVC)
        target_compile_options(${targetName} PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    else()
        target_compile_options(${targetName} PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
        )
    endif()

    if(extendedPrecacheEnableAnalysis)
        find_program(extendedPrecacheClangTidy NAMES clang-tidy REQUIRED)
        set(analysisArguments "${extendedPrecacheClangTidy};--warnings-as-errors=*")
        if(MSVC)
            # clang-tidy's tooling driver needs the exception mode explicitly as well.
            list(APPEND analysisArguments "--extra-arg=/EHsc")
        endif()
        set_target_properties(${targetName} PROPERTIES
            CXX_CLANG_TIDY "${analysisArguments}"
        )
    endif()
endfunction()
