#pragma once

#include "nextstorage_kv.h"

namespace NKikimr {

IActor* CreateBlobStorageNextStorageProxyActor(
    TGroupId groupId,
    TIntrusivePtr<IKeyValue> kv = CreateInMemoryKeyValue());

} // namespace NKikimr

