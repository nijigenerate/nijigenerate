# B031/B032: source policy; shared-root inclusion is a separately reviewed intake.
include_guard(GLOBAL)
if(NOT APPLE)
    return()
endif()
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" OR NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "The macOS native profile requires a Darwin host and target")
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR "The macOS native profile requires the installed AppleClang toolchain")
endif()
if(CMAKE_CONFIGURATION_TYPES OR NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "The macOS native profile requires a Debug or Release single-config build")
endif()
if(NICXLIVE_ENABLE_FAST_MATH OR NICXLIVE_ENABLE_AVX2)
    message(FATAL_ERROR "The macOS native profile rejects nicxlive fast-math and AVX2")
endif()

string(CONCAT macos_rejected_pattern
    "-ofast|-ffast-math|-mavx[^ \t;]*|-mfma|-march=[^ \t;]+|-mcpu=[^ \t;]+|"
    "-mdaz-ftz|-fno-exceptions|-fno-cxx-exceptions|-fno-rtti|-stdlib=libstdc\\+\\+|"
    "-ffp-contract=(on|fast|fast-honor-pragmas)|-ffp-model=(fast|aggressive)|"
    "-ffinite-math-only|-funsafe-math-optimizations|-fno-honor-nans|"
    "-fno-honor-infinities|-fno-signed-zeros|-fassociative-math|-freciprocal-math")
foreach(language IN ITEMS C CXX)
    foreach(suffix IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
        set(flag_variable "CMAKE_${language}_FLAGS${suffix}")
        string(TOLOWER "${${flag_variable}}" policy_flags)
        if(policy_flags MATCHES "(^|[ \t;])(${macos_rejected_pattern})([ \t;]|$)")
            message(FATAL_ERROR "The macOS native profile rejects incompatible options in ${flag_variable}")
        endif()
        foreach(charset IN ITEMS input exec)
            string(REGEX MATCHALL "-f${charset}-charset=[^ \t;]+" charset_options "${policy_flags}")
            foreach(charset_option IN LISTS charset_options)
                if(NOT charset_option STREQUAL "-f${charset}-charset=utf-8")
                    message(FATAL_ERROR "The macOS native profile requires UTF-8 in ${flag_variable}")
                endif()
            endforeach()
        endforeach()
    endforeach()
endforeach()

# Directory options reach project targets and locally added dependency subdirectories.
# A later target-local override still requires the generated-command audit.
add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>"
    "$<$<COMPILE_LANGUAGE:CXX>:-frtti>"
    "$<$<COMPILE_LANGUAGE:CXX>:-finput-charset=UTF-8>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fexec-charset=UTF-8>"
    -fno-fast-math
    -ffp-contract=off)
add_link_options("$<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>")
