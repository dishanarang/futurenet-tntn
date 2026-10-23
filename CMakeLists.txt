build_lib(
    LIBNAME futurenet-tntn
    SOURCE_FILES model/tntn-access-state-provider.cc
                 model/tntn-mock-access-state-provider.cc
                 model/tntn-access-selection-policy.cc
    HEADER_FILES model/tntn-access-types.h
                 model/tntn-access-state-provider.h
                 model/tntn-mock-access-state-provider.h
                 model/tntn-access-selection-policy.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libnetwork}
    TEST_SOURCES test/tntn-test-suite.cc
)
