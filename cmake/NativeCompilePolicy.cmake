# Keep native and locally compiled dependencies on the same Windows C++ ABI.
if(MSVC)
    if(NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        message(FATAL_ERROR "Native Windows builds require DLL CRT: /MDd in Debug and /MD in Release")
    endif()
    foreach(flag_variable IN ITEMS CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
        string(TOLOWER "${${flag_variable}}" policy_flags)
        if(policy_flags MATCHES "(^|[ \t;])(/fp:fast|/arch:avx[^ \t;]*|-ffast-math|-ofast|-mavx[^ \t;]*|-mfma|-march=native)([ \t;]|$)")
            message(FATAL_ERROR "Native reference compatibility rejects fast-math/AVX options in ${flag_variable}")
        endif()
    endforeach()
    add_compile_options(/EHsc /GR /utf-8 /fp:strict)
else()
    # Apply the source arithmetic policy to locally compiled dependencies too;
    # those targets do not necessarily link nijigenerate_native_options.
    add_compile_options(-ffp-contract=off -fno-fast-math)
endif()
