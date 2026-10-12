check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/futurenet-tntn-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME futurenet-tntn
    SOURCE_FILES model/futurenet-tntn.cc
                 helper/futurenet-tntn-helper.cc
    HEADER_FILES model/futurenet-tntn.h
                 helper/futurenet-tntn-helper.h
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES test/futurenet-tntn-test-suite.cc
                 ${examples_as_tests_sources}
)
