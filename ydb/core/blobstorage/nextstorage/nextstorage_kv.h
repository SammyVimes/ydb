#pragma once

#include "defs.h"

#include <functional>

namespace NKikimr {

class IKeyValue : public TThrRefBase {
public:
    using TForEachCallback = std::function<void(TStringBuf key, TStringBuf value)>;

    ~IKeyValue() override = default;

    virtual void Put(TString key, TString value) = 0;
    virtual bool Get(TStringBuf key, TString* value) const = 0;
    virtual void Erase(TStringBuf key) = 0;
    virtual void ForEachPrefix(TStringBuf prefix, TForEachCallback callback) const = 0;
};

TIntrusivePtr<IKeyValue> CreateInMemoryKeyValue();

} // namespace NKikimr

