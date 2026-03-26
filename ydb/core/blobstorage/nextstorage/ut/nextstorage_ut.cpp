#include <library/cpp/testing/unittest/registar.h>

#include <ydb/core/base/services/blobstorage_service_id.h>
#include <ydb/core/blobstorage/nextstorage/nextstorage_proxy_actor.h>
#include <ydb/core/util/actorsys_test/testactorsys.h>

namespace NKikimr {
namespace {

class TNextStorageTestContext {
private:
    static constexpr ui32 NodeId = 1;

public:
    TTestActorSystem Runtime;
    const TGroupId GroupId;
    const TActorId ProxyServiceId;
    TActorId Edge;

    TNextStorageTestContext(ui32 groupId = 1234)
        : Runtime(1)
        , GroupId(TGroupId::FromValue(groupId))
        , ProxyServiceId(MakeBlobStorageProxyID(GroupId))
        , Edge(TActorId())
    {
        Runtime.Start();
        const TActorId actorId = Runtime.Register(CreateBlobStorageNextStorageProxyActor(GroupId), NodeId);
        Runtime.RegisterService(ProxyServiceId, actorId);
        Edge = Runtime.AllocateEdgeActor(NodeId, __FILE__, __LINE__);
    }

    ~TNextStorageTestContext() {
        Runtime.Stop();
    }

    template <typename TResult>
    std::unique_ptr<TEventHandle<TResult>> SendAndWait(IEventBase* request, ui64 cookie = 0) {
        Runtime.Send(new IEventHandle(ProxyServiceId, Edge, request, 0, cookie), NodeId);
        return Runtime.WaitForEdgeActorEvent<TResult>(Edge, false);
    }

    std::unique_ptr<TEventHandle<TEvBlobStorage::TEvPutResult>> Put(const TLogoBlobID& id, TString data,
        bool issueKeepFlag = false) {
        return SendAndWait<TEvBlobStorage::TEvPutResult>(
            new TEvBlobStorage::TEvPut(id, std::move(data), TInstant::Max(), NKikimrBlobStorage::TabletLog,
                TEvBlobStorage::TEvPut::TacticDefault, issueKeepFlag));
    }

    std::unique_ptr<TEventHandle<TEvBlobStorage::TEvGetResult>> Get(const TLogoBlobID& id, ui32 shift = 0, ui32 size = 0) {
        return SendAndWait<TEvBlobStorage::TEvGetResult>(
            new TEvBlobStorage::TEvGet(id, shift, size, TInstant::Max(), NKikimrBlobStorage::FastRead));
    }
};

Y_UNIT_TEST_SUITE(NextStorage) {

    Y_UNIT_TEST(PutGetRoundtripPartialAndDuplicatePut) {
        TNextStorageTestContext ctx;

        const TString payload = "abcdef";
        const TLogoBlobID id(1, 1, 1, 0, payload.size(), 1);

        {
            auto res = ctx.Put(id, payload);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Status, NKikimrProto::OK);
        }

        {
            auto res = ctx.Put(id, payload);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Status, NKikimrProto::OK);
        }

        {
            auto res = ctx.Get(id);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Status, NKikimrProto::OK);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->ResponseSz, 1);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Responses[0].Status, NKikimrProto::OK);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Responses[0].Buffer.ConvertToString(), payload);
        }

        {
            auto res = ctx.Get(id, 2, 3);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Status, NKikimrProto::OK);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Responses[0].Status, NKikimrProto::OK);
            UNIT_ASSERT_VALUES_EQUAL(res->Get()->Responses[0].Buffer.ConvertToString(), "cde");
        }
    }

    Y_UNIT_TEST(SoftCollectGarbageWithKeepAndDoNotKeep) {
        TNextStorageTestContext ctx;

        const TLogoBlobID keepId(42, 1, 10, 0, 4, 1);
        const TLogoBlobID dropId(42, 1, 11, 0, 4, 2);

        UNIT_ASSERT_VALUES_EQUAL(ctx.Put(keepId, "keep")->Get()->Status, NKikimrProto::OK);
        UNIT_ASSERT_VALUES_EQUAL(ctx.Put(dropId, "drop")->Get()->Status, NKikimrProto::OK);

        auto keep = new TVector<TLogoBlobID>{keepId};
        auto doNotKeep = new TVector<TLogoBlobID>{dropId};
        auto gc = new TEvBlobStorage::TEvCollectGarbage(42, 1, 1, 0, true, 1, 11, keep, doNotKeep, TInstant::Max(), true);

        auto gcRes = ctx.SendAndWait<TEvBlobStorage::TEvCollectGarbageResult>(gc);
        UNIT_ASSERT_VALUES_EQUAL(gcRes->Get()->Status, NKikimrProto::OK);

        UNIT_ASSERT_VALUES_EQUAL(ctx.Get(keepId)->Get()->Responses[0].Status, NKikimrProto::OK);
        UNIT_ASSERT_VALUES_EQUAL(ctx.Get(dropId)->Get()->Responses[0].Status, NKikimrProto::NODATA);
    }

    Y_UNIT_TEST(HardBarrierIgnoresKeepFlag) {
        TNextStorageTestContext ctx;

        const TLogoBlobID id(73, 2, 1, 0, 5, 1);
        UNIT_ASSERT_VALUES_EQUAL(ctx.Put(id, "hard!", true)->Get()->Status, NKikimrProto::OK);

        auto hard = TEvBlobStorage::TEvCollectGarbage::CreateHardBarrier(73, 2, 1, 0, 2, 1, TInstant::Max());
        auto hardRes = ctx.SendAndWait<TEvBlobStorage::TEvCollectGarbageResult>(hard.Release());
        UNIT_ASSERT_VALUES_EQUAL(hardRes->Get()->Status, NKikimrProto::OK);

        auto getRes = ctx.Get(id);
        UNIT_ASSERT_VALUES_EQUAL(getRes->Get()->Responses[0].Status, NKikimrProto::NODATA);
    }

    Y_UNIT_TEST(StatusAndUnsupportedEvents) {
        TNextStorageTestContext ctx;

        {
            auto status = ctx.SendAndWait<TEvBlobStorage::TEvStatusResult>(new TEvBlobStorage::TEvStatus(TInstant::Max()));
            UNIT_ASSERT_VALUES_EQUAL(status->Get()->Status, NKikimrProto::OK);
            UNIT_ASSERT(status->Get()->StatusFlags.Check(NKikimrBlobStorage::StatusIsValid));
        }

        {
            auto block = ctx.SendAndWait<TEvBlobStorage::TEvBlockResult>(new TEvBlobStorage::TEvBlock(1, 1, TInstant::Max()));
            UNIT_ASSERT_VALUES_EQUAL(block->Get()->Status, NKikimrProto::ERROR);
            UNIT_ASSERT_STRING_CONTAINS(block->Get()->ErrorReason, "unsupported in nextstorage");
        }

        {
            auto status = ctx.SendAndWait<TEvBlobStorage::TEvStatusResult>(new TEvBlobStorage::TEvStatus(TInstant::Max()));
            UNIT_ASSERT_VALUES_EQUAL(status->Get()->Status, NKikimrProto::OK);
        }
    }
}

} // anonymous namespace
} // namespace NKikimr
