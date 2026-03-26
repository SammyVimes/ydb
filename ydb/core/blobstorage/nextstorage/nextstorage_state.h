#pragma once

#include "nextstorage_kv.h"

namespace NKikimr {

class TNextStorageState {
public:
    TNextStorageState(TGroupId groupId, TIntrusivePtr<IKeyValue> kv);

    std::unique_ptr<TEvBlobStorage::TEvPutResult> Handle(TEvBlobStorage::TEvPut* msg);
    std::unique_ptr<TEvBlobStorage::TEvGetResult> Handle(TEvBlobStorage::TEvGet* msg);
    std::unique_ptr<TEvBlobStorage::TEvCollectGarbageResult> Handle(TEvBlobStorage::TEvCollectGarbage* msg);
    std::unique_ptr<TEvBlobStorage::TEvStatusResult> Handle(TEvBlobStorage::TEvStatus* msg) const;

    struct TBlobRecord {
        TString Buffer;
        bool Keep = false;
        bool DoNotKeep = false;
    };

    struct TBarrier {
        ui32 RecordGeneration = 0;
        ui32 PerGenerationCounter = 0;
        ui32 CollectGeneration = 0;
        ui32 CollectStep = 0;

        std::pair<ui32, ui32> CollectPair() const {
            return {CollectGeneration, CollectStep};
        }
    };

private:
    bool LoadBlob(const TLogoBlobID& id, TBlobRecord* record) const;
    void SaveBlob(const TLogoBlobID& id, const TBlobRecord& record);

    bool LoadBlockGeneration(ui64 tabletId, ui32* generation) const;
    bool IsBlocked(ui64 tabletId, ui32 generation) const;

    bool LoadBarrier(bool hard, ui64 tabletId, ui32 channel, TBarrier* barrier) const;
    void SaveBarrier(bool hard, ui64 tabletId, ui32 channel, const TBarrier& barrier);

    bool IsCollectedByBarrier(const TLogoBlobID& id, bool issueKeepFlag = false) const;
    void DoCollection(ui64 tabletId, ui32 channel, bool force, ui32 generation, ui32 step);
    bool ValidateBarrierUpdate(bool hard, TEvBlobStorage::TEvCollectGarbage* msg, TString* errorReason) const;

private:
    const TGroupId GroupId;
    const TIntrusivePtr<IKeyValue> Kv;
    TStorageStatusFlags StorageStatusFlags;
};

} // namespace NKikimr
