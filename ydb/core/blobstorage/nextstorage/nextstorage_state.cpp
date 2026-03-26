#include "nextstorage_state.h"

#include <cstring>
#include <limits>

#include <util/string/builder.h>

namespace NKikimr {
namespace {

constexpr TStringBuf BlobPrefix = "blob/";
constexpr TStringBuf BlockPrefix = "block/";
constexpr TStringBuf BarrierPrefix = "barrier/";
constexpr TStringBuf HardBarrierPrefix = "hard_barrier/";

constexpr ui8 KeepMask = 1;
constexpr ui8 DoNotKeepMask = 2;

TString MakeBlobKey(const TLogoBlobID& id) {
    TString key;
    key.reserve(BlobPrefix.size() + TLogoBlobID::BinarySize);
    key.append(BlobPrefix.data(), BlobPrefix.size());
    key += id.AsBinaryString();
    return key;
}

bool ParseBlobKey(TStringBuf key, TLogoBlobID* id) {
    if (key.size() != BlobPrefix.size() + TLogoBlobID::BinarySize) {
        return false;
    }
    if (key.SubStr(0, BlobPrefix.size()) != BlobPrefix) {
        return false;
    }
    *id = TLogoBlobID::FromBinary(key.SubStr(BlobPrefix.size()));
    return true;
}

TString MakeBlockKey(ui64 tabletId) {
    return TStringBuilder() << BlockPrefix << tabletId;
}

TString MakeBarrierKey(bool hard, ui64 tabletId, ui32 channel) {
    return TStringBuilder() << (hard ? HardBarrierPrefix : BarrierPrefix) << tabletId << '/' << channel;
}

template <typename T>
void AppendPod(TString* out, const T& value) {
    out->append(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadPod(TStringBuf value, size_t offset, T* out) {
    if (offset + sizeof(T) > value.size()) {
        return false;
    }
    memcpy(out, value.data() + offset, sizeof(T));
    return true;
}

TString SerializeBlob(const TNextStorageState::TBlobRecord& blob) {
    TString value;
    value.reserve(1 + blob.Buffer.size());

    ui8 flags = 0;
    if (blob.Keep) {
        flags |= KeepMask;
    }
    if (blob.DoNotKeep) {
        flags |= DoNotKeepMask;
    }

    value.push_back(static_cast<char>(flags));
    value.append(blob.Buffer);
    return value;
}

bool DeserializeBlob(TStringBuf value, TNextStorageState::TBlobRecord* blob) {
    if (value.empty()) {
        return false;
    }

    const ui8 flags = static_cast<ui8>(value[0]);
    blob->Keep = flags & KeepMask;
    blob->DoNotKeep = flags & DoNotKeepMask;
    blob->Buffer.assign(value.data() + 1, value.size() - 1);
    return true;
}

TString SerializeBarrier(const TNextStorageState::TBarrier& barrier) {
    TString value;
    value.reserve(4 * sizeof(ui32));
    AppendPod(&value, barrier.RecordGeneration);
    AppendPod(&value, barrier.PerGenerationCounter);
    AppendPod(&value, barrier.CollectGeneration);
    AppendPod(&value, barrier.CollectStep);
    return value;
}

bool DeserializeBarrier(TStringBuf value, TNextStorageState::TBarrier* barrier) {
    return ReadPod(value, 0, &barrier->RecordGeneration) &&
        ReadPod(value, sizeof(ui32), &barrier->PerGenerationCounter) &&
        ReadPod(value, 2 * sizeof(ui32), &barrier->CollectGeneration) &&
        ReadPod(value, 3 * sizeof(ui32), &barrier->CollectStep);
}

std::pair<ui32, ui32> MakeGenStep(const TLogoBlobID& id) {
    return {id.Generation(), id.Step()};
}

} // anonymous namespace

TNextStorageState::TNextStorageState(TGroupId groupId, TIntrusivePtr<IKeyValue> kv)
    : GroupId(groupId)
    , Kv(std::move(kv))
    , StorageStatusFlags(NKikimrBlobStorage::StatusIsValid)
{
    Y_ABORT_UNLESS(Kv);
}

bool TNextStorageState::LoadBlob(const TLogoBlobID& id, TBlobRecord* record) const {
    TString value;
    if (!Kv->Get(MakeBlobKey(id), &value)) {
        return false;
    }
    return DeserializeBlob(value, record);
}

void TNextStorageState::SaveBlob(const TLogoBlobID& id, const TBlobRecord& record) {
    Kv->Put(MakeBlobKey(id), SerializeBlob(record));
}

bool TNextStorageState::LoadBlockGeneration(ui64 tabletId, ui32* generation) const {
    TString value;
    if (!Kv->Get(MakeBlockKey(tabletId), &value)) {
        return false;
    }
    return ReadPod(value, 0, generation);
}

bool TNextStorageState::IsBlocked(ui64 tabletId, ui32 generation) const {
    ui32 blockedGeneration = 0;
    return LoadBlockGeneration(tabletId, &blockedGeneration) && generation <= blockedGeneration;
}

bool TNextStorageState::LoadBarrier(bool hard, ui64 tabletId, ui32 channel, TBarrier* barrier) const {
    TString value;
    if (!Kv->Get(MakeBarrierKey(hard, tabletId, channel), &value)) {
        return false;
    }
    return DeserializeBarrier(value, barrier);
}

void TNextStorageState::SaveBarrier(bool hard, ui64 tabletId, ui32 channel, const TBarrier& barrier) {
    Kv->Put(MakeBarrierKey(hard, tabletId, channel), SerializeBarrier(barrier));
}

bool TNextStorageState::IsCollectedByBarrier(const TLogoBlobID& id, bool issueKeepFlag) const {
    TBarrier hardBarrier;
    if (LoadBarrier(true, id.TabletID(), id.Channel(), &hardBarrier) && MakeGenStep(id) <= hardBarrier.CollectPair()) {
        return true;
    }

    if (issueKeepFlag) {
        return false;
    }

    TBarrier softBarrier;
    return LoadBarrier(false, id.TabletID(), id.Channel(), &softBarrier) && MakeGenStep(id) <= softBarrier.CollectPair();
}

void TNextStorageState::DoCollection(ui64 tabletId, ui32 channel, bool force, ui32 generation, ui32 step) {
    const auto collectPair = std::make_pair(generation, step);
    TVector<TString> keysToErase;

    Kv->ForEachPrefix(BlobPrefix, [&](TStringBuf key, TStringBuf value) {
        TLogoBlobID id;
        if (!ParseBlobKey(key, &id) || id.TabletID() != tabletId || id.Channel() != channel || MakeGenStep(id) > collectPair) {
            return;
        }

        TBlobRecord blob;
        if (!DeserializeBlob(value, &blob)) {
            return;
        }

        if (force || blob.DoNotKeep || !blob.Keep) {
            keysToErase.push_back(TString(key));
        }
    });

    for (const TString& key : keysToErase) {
        Kv->Erase(key);
    }
}

bool TNextStorageState::ValidateBarrierUpdate(bool hard, TEvBlobStorage::TEvCollectGarbage* msg, TString* errorReason) const {
    TBarrier current;
    if (!LoadBarrier(hard, msg->TabletId, msg->Channel, &current)) {
        return true;
    }

    const auto currentRecord = std::make_pair(current.RecordGeneration, current.PerGenerationCounter);
    const auto newRecord = std::make_pair(msg->RecordGeneration, msg->PerGenerationCounter);
    const auto currentCollect = current.CollectPair();
    const auto newCollect = std::make_pair(msg->CollectGeneration, msg->CollectStep);

    if (newRecord == currentRecord) {
        if (newCollect != currentCollect) {
            *errorReason = "barrier update has same record but different collect point";
            return false;
        }
        return true;
    }

    if (newRecord < currentRecord) {
        *errorReason = "barrier update has outdated record generation/counter";
        return false;
    }

    if (newCollect < currentCollect) {
        *errorReason = "barrier update attempts to move collect point backwards";
        return false;
    }

    return true;
}

std::unique_ptr<TEvBlobStorage::TEvPutResult> TNextStorageState::Handle(TEvBlobStorage::TEvPut* msg) {
    if (msg->Id != msg->Id.FullID()) {
        return msg->MakeErrorResponse(NKikimrProto::ERROR, "part ids are not supported in nextstorage", GroupId);
    }

    if (!msg->IgnoreBlock && IsBlocked(msg->Id.TabletID(), msg->Id.Generation())) {
        return std::make_unique<TEvBlobStorage::TEvPutResult>(NKikimrProto::BLOCKED, msg->Id, StorageStatusFlags, GroupId, 0.f);
    }

    for (const auto& [tabletId, generation] : msg->ExtraBlockChecks) {
        if (IsBlocked(tabletId, generation)) {
            return std::make_unique<TEvBlobStorage::TEvPutResult>(NKikimrProto::BLOCKED, msg->Id, StorageStatusFlags, GroupId, 0.f);
        }
    }

    if (IsCollectedByBarrier(msg->Id, msg->IssueKeepFlag)) {
        return msg->MakeErrorResponse(NKikimrProto::ERROR, "blob is behind barrier", GroupId);
    }

    const TString buffer = msg->Buffer.ConvertToString();
    TBlobRecord blob;
    if (LoadBlob(msg->Id, &blob)) {
        if (blob.Buffer != buffer) {
            return msg->MakeErrorResponse(NKikimrProto::ERROR, "duplicate put with different payload", GroupId);
        }
    } else {
        blob.Buffer = buffer;
    }

    if (msg->IssueKeepFlag) {
        blob.Keep = true;
        blob.DoNotKeep = false;
    }

    SaveBlob(msg->Id, blob);
    return std::make_unique<TEvBlobStorage::TEvPutResult>(NKikimrProto::OK, msg->Id, StorageStatusFlags, GroupId, 0.f);
}

std::unique_ptr<TEvBlobStorage::TEvGetResult> TNextStorageState::Handle(TEvBlobStorage::TEvGet* msg) {
    if (const auto& reader = msg->ReaderTabletData; reader && IsBlocked(reader->Id, reader->Generation)) {
        return msg->MakeErrorResponse(NKikimrProto::BLOCKED, "block race detected", GroupId);
    }

    auto result = std::make_unique<TEvBlobStorage::TEvGetResult>(NKikimrProto::OK, msg->QuerySize, GroupId);
    for (ui32 i = 0; i < msg->QuerySize; ++i) {
        const auto& query = msg->Queries[i];
        auto& response = result->Responses[i];

        response.Id = query.Id;
        response.Shift = query.Shift;
        response.RequestedSize = query.Size;

        TBlobRecord blob;
        if (!LoadBlob(query.Id, &blob)) {
            response.Status = NKikimrProto::NODATA;
            continue;
        }

        response.Status = NKikimrProto::OK;
        response.Keep = blob.Keep;
        response.DoNotKeep = blob.DoNotKeep;

        const ui32 maxSize = query.Shift < blob.Buffer.size() ? blob.Buffer.size() - query.Shift : 0;
        const ui32 requestedSize = query.Size ? query.Size : std::numeric_limits<ui32>::max();
        const ui32 size = Min(maxSize, requestedSize);
        const ui32 offset = size ? query.Shift : 0;
        response.Buffer = TRope(blob.Buffer.substr(offset, size));
    }

    return result;
}

std::unique_ptr<TEvBlobStorage::TEvCollectGarbageResult> TNextStorageState::Handle(TEvBlobStorage::TEvCollectGarbage* msg) {
    ui32 blockedGeneration = 0;
    const bool hasBlock = LoadBlockGeneration(msg->TabletId, &blockedGeneration);
    if (!msg->IgnoreBlock && hasBlock && msg->RecordGeneration <= blockedGeneration &&
        (msg->CollectGeneration != std::numeric_limits<ui32>::max() ||
            msg->CollectStep != std::numeric_limits<ui32>::max() ||
            blockedGeneration != std::numeric_limits<ui32>::max())) {
        return std::make_unique<TEvBlobStorage::TEvCollectGarbageResult>(NKikimrProto::BLOCKED, msg->TabletId,
            msg->RecordGeneration, msg->PerGenerationCounter, msg->Channel);
    }

    if (msg->Hard) {
        if (msg->Keep || msg->DoNotKeep || !msg->Collect) {
            return msg->MakeErrorResponse(NKikimrProto::ERROR, "hard collect requires collect=true and no flags", GroupId);
        }
    } else {
        if (msg->Keep) {
            for (const TLogoBlobID& id : *msg->Keep) {
                TBlobRecord blob;
                if (LoadBlob(id, &blob)) {
                    blob.Keep = true;
                    blob.DoNotKeep = false;
                    SaveBlob(id, blob);
                }
            }
        }

        if (msg->DoNotKeep) {
            for (const TLogoBlobID& id : *msg->DoNotKeep) {
                TBlobRecord blob;
                if (LoadBlob(id, &blob)) {
                    blob.DoNotKeep = true;
                    SaveBlob(id, blob);
                }
            }
        }
    }

    if (msg->Collect) {
        TString errorReason;
        if (!ValidateBarrierUpdate(msg->Hard, msg, &errorReason)) {
            return msg->MakeErrorResponse(NKikimrProto::ERROR, errorReason, GroupId);
        }

        SaveBarrier(msg->Hard, msg->TabletId, msg->Channel, TBarrier{
            msg->RecordGeneration,
            msg->PerGenerationCounter,
            msg->CollectGeneration,
            msg->CollectStep,
        });

        DoCollection(msg->TabletId, msg->Channel, msg->Hard, msg->CollectGeneration, msg->CollectStep);
    }

    return std::make_unique<TEvBlobStorage::TEvCollectGarbageResult>(NKikimrProto::OK, msg->TabletId,
        msg->RecordGeneration, msg->PerGenerationCounter, msg->Channel);
}

std::unique_ptr<TEvBlobStorage::TEvStatusResult> TNextStorageState::Handle(TEvBlobStorage::TEvStatus*) const {
    return std::make_unique<TEvBlobStorage::TEvStatusResult>(NKikimrProto::OK, StorageStatusFlags);
}

} // namespace NKikimr
