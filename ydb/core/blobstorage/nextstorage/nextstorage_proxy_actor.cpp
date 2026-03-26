#include "nextstorage_proxy_actor.h"

#include "nextstorage_state.h"

#include <ydb/core/blobstorage/vdisk/common/vdisk_events.h>

namespace NKikimr {
namespace {

class TBlobStorageNextStorageProxyActor : public TActor<TBlobStorageNextStorageProxyActor> {
private:
    const TGroupId GroupId;
    TNextStorageState State;

    template <typename TResponse, typename TRequest>
    static TResponse* CopyExecutionRelay(TRequest* request, TResponse* response) {
        response->ExecutionRelay = std::move(request->ExecutionRelay);
        return response;
    }

    template <typename TPtr>
    void ReplyUnsupported(TPtr& ev) {
        auto response = ev->Get()->MakeErrorResponse(NKikimrProto::ERROR, "unsupported in nextstorage", GroupId);
        Send(ev->Sender, CopyExecutionRelay(ev->Get(), response.release()), 0, ev->Cookie);
    }

    void Handle(TEvBlobStorage::TEvPut::TPtr& ev) {
        auto response = State.Handle(ev->Get());
        Send(ev->Sender, CopyExecutionRelay(ev->Get(), response.release()), 0, ev->Cookie);
    }

    void Handle(TEvBlobStorage::TEvGet::TPtr& ev) {
        auto response = State.Handle(ev->Get());
        Send(ev->Sender, CopyExecutionRelay(ev->Get(), response.release()), 0, ev->Cookie);
    }

    void Handle(TEvBlobStorage::TEvCollectGarbage::TPtr& ev) {
        auto response = State.Handle(ev->Get());
        Send(ev->Sender, CopyExecutionRelay(ev->Get(), response.release()), 0, ev->Cookie);
    }

    void Handle(TEvBlobStorage::TEvStatus::TPtr& ev) {
        auto response = State.Handle(ev->Get());
        Send(ev->Sender, CopyExecutionRelay(ev->Get(), response.release()), 0, ev->Cookie);
    }

    void Handle(TEvBlobStorage::TEvBlock::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvGetBlock::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvDiscover::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvRange::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvPatch::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvInplacePatch::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvAssimilate::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvCheckIntegrity::TPtr& ev) {
        ReplyUnsupported(ev);
    }

    void Handle(TEvBlobStorage::TEvConfigureProxy::TPtr&) {
        // Intentionally ignored: nextstorage has static group configuration in this increment.
    }

    void Handle(TEvents::TEvPoisonPill::TPtr& ev) {
        Send(ev->Sender, new TEvents::TEvPoisonTaken);
        PassAway();
    }

private:
    STATEFN(StateFunc) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvBlobStorage::TEvPut, Handle);
            hFunc(TEvBlobStorage::TEvGet, Handle);
            hFunc(TEvBlobStorage::TEvCollectGarbage, Handle);
            hFunc(TEvBlobStorage::TEvStatus, Handle);

            hFunc(TEvBlobStorage::TEvBlock, Handle);
            hFunc(TEvBlobStorage::TEvGetBlock, Handle);
            hFunc(TEvBlobStorage::TEvDiscover, Handle);
            hFunc(TEvBlobStorage::TEvRange, Handle);
            hFunc(TEvBlobStorage::TEvPatch, Handle);
            hFunc(TEvBlobStorage::TEvInplacePatch, Handle);
            hFunc(TEvBlobStorage::TEvAssimilate, Handle);
            hFunc(TEvBlobStorage::TEvCheckIntegrity, Handle);

            hFunc(TEvBlobStorage::TEvConfigureProxy, Handle);
            hFunc(TEvents::TEvPoisonPill, Handle);

            default:
                break;
        }
    }

public:
    static constexpr NKikimrServices::TActivity::EType ActorActivityType() {
        return NKikimrServices::TActivity::BS_PROXY_ACTOR;
    }

    TBlobStorageNextStorageProxyActor(TGroupId groupId, TIntrusivePtr<IKeyValue> kv)
        : TActor(&TThis::StateFunc)
        , GroupId(groupId)
        , State(groupId, std::move(kv))
    {}
};

} // anonymous namespace

IActor* CreateBlobStorageNextStorageProxyActor(TGroupId groupId, TIntrusivePtr<IKeyValue> kv) {
    return new TBlobStorageNextStorageProxyActor(groupId, std::move(kv));
}

} // namespace NKikimr

