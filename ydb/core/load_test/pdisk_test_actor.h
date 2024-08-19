#pragma once

#include "defs.h"
#include <ydb/core/base/blobstorage.h>

namespace NKikimr {
    enum {
        EvStopTest = EventSpaceBegin(TKikimrEvents::ES_PRIVATE),
        EvUpdateQuantile,
        EvUpdateMonitoring,
    };

    constexpr ui64 MonitoringUpdateCycleMs = 1000;

    constexpr TDuration DelayBeforeMeasurements = TDuration::Seconds(15);

    struct TEvUpdateMonitoring : TEventLocal<TEvUpdateMonitoring, EvUpdateMonitoring>
    {};

    class TLoadActorException : public yexception
    {};

    NActors::IActor *CreatePDiskWriterLoadTest(const NKikimr::TEvLoadTestRequest::TPDiskWriteLoad& cmd,
            const NActors::TActorId& parent, const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters,
            ui64 index, ui64 tag, ui64 diskSizeBytes);

    NActors::IActor *CreatePDiskLogWriterLoadTest(const NKikimr::TEvLoadTestRequest::TPDiskLogLoad& cmd,
            const NActors::TActorId& parent, const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters,
            ui64 index, ui64 tag);

    NActors::IActor *CreatePDiskReaderLoadTest(const NKikimr::TEvLoadTestRequest::TPDiskReadLoad& cmd,
            const NActors::TActorId& parent, const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters,
            ui64 index, ui64 tag);

#define VERIFY_PARAM2(FIELD, NAME) \
    do { \
        if (!(FIELD).Has##NAME()) { \
            ythrow TLoadActorException() << "missing " << #NAME << " parameter"; \
        } \
    } while (false)

#define VERIFY_PARAM(NAME) VERIFY_PARAM2(cmd, NAME)
}