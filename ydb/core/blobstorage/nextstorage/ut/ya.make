UNITTEST_FOR(ydb/core/blobstorage/nextstorage)

SIZE(MEDIUM)

PEERDIR(
    library/cpp/getopt
    library/cpp/svnversion
    library/cpp/testing/unittest
    ydb/core/base
    ydb/core/base/services
    ydb/core/blobstorage/nextstorage
    ydb/core/util/actorsys_test
)

SRCS(
    nextstorage_ut.cpp
)

END()

