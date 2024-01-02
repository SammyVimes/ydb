#include "blobstorage_pdisk_log_cache.h"

namespace NKikimr {
namespace NPDisk {

TLogCache::TCacheRecord::TCacheRecord(ui64 offset, TRcBuf data, ui64 commitStateSeqNo, TVector<ui64> badOffsets)
    : Offset(offset)
    , Data(std::move(data))
    , CommitStateSeqNo(commitStateSeqNo)
    , BadOffsets(std::move(badOffsets))
{}

TLogCache::TCacheRecord::TCacheRecord(TCacheRecord&& other)
    : Offset(other.Offset)
    , Data(std::move(other.Data))
    , CommitStateSeqNo(other.CommitStateSeqNo)
    , BadOffsets(std::move(other.BadOffsets))
{}

size_t TLogCache::Size() const {
    return Index_.size();
}

template <typename C>
typename C::iterator
FindKeyLess(C& c, const typename C::key_type& key) {
    auto iter = c.lower_bound(key);

    if (iter == c.begin()) {
        return c.end();
    }

    return --iter;
}

template <typename C>
typename C::iterator
FindKeyLessEqual(C& c, const typename C::key_type& key) {
    auto iter = c.upper_bound(key);

    if (iter == c.begin()) {
        return c.end();
    }

    return --iter;
}

std::tuple<bool, TVector<ui64>> TLogCache::Find(ui64 offset, ui32 size, ui64 commitStateSeqNo, char* buffer) {
    if (Disabled_) {
        return make_tuple(false, TVector<ui64>());
    }

    TVector<TCacheRecord*> res;

    auto indexIt = FindKeyLessEqual(Index_, offset);

    if (indexIt == Index_.end()) {
        return make_tuple(false, TVector<ui64>());
    }

    ui64 cur = offset;
    ui64 end = offset + size;

    while (indexIt != Index_.end() && cur < end) {
        if (indexIt->second.CommitStateSeqNo != commitStateSeqNo) {
            return make_tuple(false, TVector<ui64>());
        }

        ui64 recStart = indexIt->first;
        ui64 recEnd = recStart + indexIt->second.Data.Size();

        if (cur >= recStart && cur < recEnd) {
            res.push_back(&indexIt->second);
        } else {
            return make_tuple(false, TVector<ui64>());
        }

        cur = recEnd;

        indexIt++;
    }

    if (cur < end) {
        return make_tuple(false, TVector<ui64>());
    }

    TVector<ui64> badOffsets;

    for (auto cacheRecord : res) {
        ui64 recStart = cacheRecord->Offset;
        ui64 recEnd = recStart + cacheRecord->Data.Size();

        // Determine the buffer's chunk start and end absolute offsets.
        ui64 chunkStartOffset = std::max(recStart, offset);
        ui64 chunkEndOffset = std::min(recEnd, offset + size);
        ui64 chunkSize = chunkEndOffset - chunkStartOffset;

        // Calculate the chunk's position within the buffer to start copying.
        ui64 chunkOffset = chunkStartOffset - recStart;

        // Copy the chunk data to the buffer.
        std::memcpy(buffer + (chunkStartOffset - offset), cacheRecord->Data.Data() + chunkOffset, chunkSize);

        badOffsets.insert(badOffsets.end(), cacheRecord->BadOffsets.begin(), cacheRecord->BadOffsets.end());
    }

    return make_tuple(true, std::move(badOffsets));
}

std::pair<i64, i64> TLogCache::PrepareInsertion(ui64 start, ui32 size) {
    ui64 end = start + size;
    ui32 leftPadding = 0;
    ui32 rightPadding = 0;

    // Check if there is a block that overlaps with the new insertion's start.
    auto it1 = FindKeyLessEqual(Index_, start);
    if (it1 != Index_.end()) {
        ui64 maybeStart = it1->first;
        ui64 maybeEnd = maybeStart + it1->second.Data.Size();

        if (start < maybeEnd) {
            if (end <= maybeEnd) {
                return {-1, -1}; // There is an overlapping block; return {-1, -1} to indicate it.
            }
            leftPadding = maybeEnd - start;
        }
    }

    ui64 offsetStart = start + leftPadding;

    // Check if there is a block that overlaps with the new insertion's end.
    auto it2 = FindKeyLess(Index_, end);
    if (it2 != Index_.end()) {
        ui64 dataSize = it2->second.Data.Size();

        ui64 maybeStart = it2->first;
        ui64 maybeEnd = maybeStart + dataSize;

        if (offsetStart == maybeStart) {
            // There is an overlapping block; return {-1, -1} to indicate it.
            if (end <= maybeEnd) {
                return {-1, -1};
            }

            leftPadding += dataSize;
            offsetStart += dataSize;
        }

        if (end <= maybeEnd) {
            rightPadding = end - maybeStart;
        }
    }

    ui64 offsetEnd = start + (size - rightPadding);

    // Remove any blocks that are completely covered by the new insertion.
    auto it = Index_.lower_bound(offsetStart);
    while (it != Index_.end()) {
        ui64 blockEnd = it->first + it->second.Data.Size();
        if (blockEnd < offsetEnd) {
            it = Index_.erase(it);
        } else {
            if (offsetStart == it->first && offsetEnd == blockEnd) {
                return {-1, -1};
            }
            break;
        }
    }

    return {leftPadding, rightPadding};
}

bool TLogCache::Insert(const char* dataPtr, ui64 offset, ui32 size, ui64 commitStateSeqNo, const TVector<ui64>& badOffsets) {
    if (Disabled_) {
        return false;
    }

    auto [leftPadding, rightPadding] = PrepareInsertion(offset, size);

    if (leftPadding == -1 && rightPadding == -1) {
        return false;
    }

    const char* dataStart = dataPtr + leftPadding;
    const char* dataEnd = dataPtr + (size - rightPadding);

    Y_DEBUG_ABORT_UNLESS(dataStart < dataEnd);

    auto [_, inserted] = Index_.try_emplace(offset + leftPadding, std::move(TLogCache::TCacheRecord(
            offset + leftPadding,
            TRcBuf(TString(dataStart, dataEnd)),
            commitStateSeqNo,
            badOffsets)
    ));

    auto generateMessage = [this, offset, size, lp=leftPadding, rp=rightPadding]() {
        TStringStream stream;

        stream << "offset=" << offset << " size=" << size << " lp=" << lp << " rp=" << rp;

        for (auto it = Index_.begin(); it != Index_.end(); ++it) {
            auto offset = it->first;
            auto sz = it->second.Data.Size();
            stream << " # " << offset << " -> " << sz;
        }

        return stream.Str();
    };

    Y_UNUSED(generateMessage);
    if (!inserted) {
        TStringBuilder builder;
        builder << generateMessage();
        Y_DEBUG_ABORT_UNLESS(false, "%s", builder.c_str());
    }

    return true;
}

size_t TLogCache::EraseRange(ui64 begin, ui64 end) {
    Y_DEBUG_ABORT_UNLESS(begin <= end);
    auto beginIt = Index_.lower_bound(begin);
    auto endIt = Index_.lower_bound(end);
    size_t dist = std::distance(beginIt, endIt);
    Index_.erase(beginIt, endIt);
    return dist;
}

void TLogCache::Clear() {
    Index_.clear();
}

void TLogCache::Disable() {
    Clear();

    Disabled_ = true;
}

} // NPDisk
} // NKikimr