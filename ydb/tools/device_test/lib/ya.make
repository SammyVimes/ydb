LIBRARY(kikimr_device_test)

OWNER(va-kuznecov)

PEERDIR(
    contrib/libs/protobuf
    ydb/tools/device_test/proto
    library/cpp/monlib/dynamic_counters/percentile
    ydb/library/actors/core
    ydb/core/blobstorage/lwtrace_probes
    ydb/core/load_test
    ydb/core/protos
)

SRCS(
    ../device_test_tool.h
    ../device_test_tool_aio_test.h
    ../device_test_tool_driveestimator.h
    ../device_test_tool_trim_test.cpp
    ../device_test_tool_trim_test.h
    ../device_test_tool_pdisk_test.h
)

END()

RECURSE_FOR_TESTS(
    ../ut
)
