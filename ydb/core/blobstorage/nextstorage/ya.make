LIBRARY()

SRCS(
    defs.h
    nextstorage_kv.h
    nextstorage_kv_inmemory.h
    nextstorage_kv_inmemory.cpp
    nextstorage_state.h
    nextstorage_state.cpp
    nextstorage_proxy_actor.h
    nextstorage_proxy_actor.cpp
)

PEERDIR(
    ydb/core/base
    ydb/core/base/services
    ydb/core/blobstorage/vdisk/common
)

END()

RECURSE_FOR_TESTS(
    ut
)

