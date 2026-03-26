#include "nextstorage_kv_inmemory.h"

namespace NKikimr {

void TInMemoryKeyValue::Put(TString key, TString value) {
    Data[std::move(key)] = std::move(value);
}

bool TInMemoryKeyValue::Get(TStringBuf key, TString* value) const {
    const auto it = Data.find(TString(key));
    if (it == Data.end()) {
        return false;
    }
    *value = it->second;
    return true;
}

void TInMemoryKeyValue::Erase(TStringBuf key) {
    Data.erase(TString(key));
}

void TInMemoryKeyValue::ForEachPrefix(TStringBuf prefix, TForEachCallback callback) const {
    for (const auto& [key, value] : Data) {
        const TStringBuf currentKey = key;
        if (currentKey.size() >= prefix.size() && currentKey.SubStr(0, prefix.size()) == prefix) {
            callback(currentKey, value);
        }
    }
}

TIntrusivePtr<IKeyValue> CreateInMemoryKeyValue() {
    return MakeIntrusive<TInMemoryKeyValue>();
}

} // namespace NKikimr

