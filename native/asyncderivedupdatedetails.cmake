if(NIJIGENERATE_IMGUI AND TARGET nicxlive::nicxlive)
    add_library(nijigenerate_asyncderivedupdatedetails STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/asyncderivedupdatedetails.cpp")
    target_sources(nijigenerate_asyncderivedupdatedetails PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/asyncderivedupdatedetails_sort.hpp")
    target_include_directories(nijigenerate_asyncderivedupdatedetails PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_asyncderivedupdatedetails PUBLIC
        FILE_SET asyncderivedupdatedetails_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/asyncderivedupdatedetails.hpp")
    target_link_libraries(nijigenerate_asyncderivedupdatedetails
        PUBLIC nijigenerate_native_options nicxlive::nicxlive
        PRIVATE nijigenerate_core nijigenerate_button nijigenerate_label nijigenerate_tooltip)
endif()
