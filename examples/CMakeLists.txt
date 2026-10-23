build_lib_example(
    NAME tntn-example-basic
    SOURCE_FILES tntn-example-basic.cc
    LIBRARIES_TO_LINK ${libfuturenet-tntn}
)

build_lib_example(
    NAME tntn-policy-demo
    SOURCE_FILES tntn-policy-demo.cc
    LIBRARIES_TO_LINK ${libfuturenet-tntn}
)
