OWNER(va-kuznecov g:kikimr)

IF (SANITIZER_TYPE AND AUTOCHECK)

ELSE()

UNITTEST_FOR(kikimr/tools/device_test/lib)

SIZE(LARGE)
TIMEOUT(3600)
TAG(ya:fat)

SRC(
    ../device_test_tool_ut.cpp
)

PEERDIR(
    contrib/ydb/library/yql/parser/pg_wrapper
    contrib/ydb/library/yql/sql/pg
    contrib/ydb/library/yql/minikql/comp_nodes/llvm
)

END()
ENDIF()
