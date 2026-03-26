#pragma once

#include "nextstorage_kv.h"

namespace NKikimr {

class TInMemoryKeyValue final : public IKeyValue {
public:
    void Put(TString key, TString value) override;
    bool Get(TStringBuf key, TString* value) const override;
    void Erase(TStringBuf key) override;
    void ForEachPrefix(TStringBuf prefix, TForEachCallback callback) const override;

private:
    THashMap<TString, TString> Data;
};

} // namespace NKikimr

