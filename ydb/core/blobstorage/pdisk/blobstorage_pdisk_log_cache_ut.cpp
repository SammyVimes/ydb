#include "blobstorage_pdisk_log_cache.h"

#include <library/cpp/testing/unittest/registar.h>

namespace NKikimr {
namespace NPDisk {

bool FindInCache(TLogCache& cache, ui64 offset, ui32 size, char* buffer) {
    return std::get<0>(cache.Find(offset, size, 1, buffer));
}

bool InsertIntoCache(TLogCache& cache, const char* data, ui64 offset, ui32 size) {
    return cache.Insert(data, offset, size, 1);
}

Y_UNIT_TEST_SUITE(TLogCache) {
    Y_UNIT_TEST(Simple) {
        TLogCache cache;
        char buf[2] = {};

        UNIT_ASSERT(InsertIntoCache(cache, "a", 1, 1));
        UNIT_ASSERT(InsertIntoCache(cache, "b", 2, 1));
        UNIT_ASSERT_EQUAL(cache.Size(), 2);
        UNIT_ASSERT(FindInCache(cache, 1, 1, buf));
        UNIT_ASSERT_STRINGS_EQUAL(buf, "a");

        UNIT_ASSERT(InsertIntoCache(cache, "c", 3, 1));
        UNIT_ASSERT_EQUAL(1, cache.EraseRange(2, 3));  // 2 was removed
        UNIT_ASSERT_EQUAL(cache.Size(), 2);
        UNIT_ASSERT(!FindInCache(cache, 2, 1, buf));
        UNIT_ASSERT(FindInCache(cache, 3, 1, buf));
        UNIT_ASSERT_STRINGS_EQUAL(buf, "c");

        UNIT_ASSERT_EQUAL(1, cache.EraseRange(1, 2));  // 1 was removed
        UNIT_ASSERT(InsertIntoCache(cache, "d", 4, 1));

        UNIT_ASSERT_EQUAL(cache.Size(), 2);
        UNIT_ASSERT(!FindInCache(cache, 1, 1, buf));
        UNIT_ASSERT(FindInCache(cache, 4, 1, buf));
        UNIT_ASSERT_STRINGS_EQUAL(buf, "d");

        UNIT_ASSERT_EQUAL(1, cache.EraseRange(3, 4));  // 3 was removed
        UNIT_ASSERT_EQUAL(cache.Size(), 1);
        UNIT_ASSERT(!FindInCache(cache, 3, 1, buf));
        UNIT_ASSERT(FindInCache(cache, 4, 1, buf));
        UNIT_ASSERT_STRINGS_EQUAL(buf, "d");


        UNIT_ASSERT_EQUAL(1, cache.EraseRange(4, 5));
        UNIT_ASSERT_EQUAL(cache.Size(), 0);
    }

    Y_UNIT_TEST(DoubleInsertion) {
        TLogCache cache;
        
        char buf[5] = {};

        auto checkFn = [&]() {
            UNIT_ASSERT_EQUAL(25, cache.Size());
            
            for (int i = 0; i < 100; i += 4) {
                UNIT_ASSERT(FindInCache(cache, i, 4, buf));
                UNIT_ASSERT_STRINGS_EQUAL(buf, "abcd");
            }
        };

        for (int i = 0; i < 100; i += 4) {
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", i, 4));
        }

        checkFn();
        
        for (int i = 0; i < 100; i += 4) {
            UNIT_ASSERT(!InsertIntoCache(cache, "abcd", i, 4));
        }

        checkFn();
    }

    Y_UNIT_TEST(FullyOverlapping) {
        TLogCache cache;
        
        InsertIntoCache(cache, "abcd", 0, 4);
        UNIT_ASSERT_EQUAL(1, cache.Size());

        UNIT_ASSERT(!InsertIntoCache(cache, "bc", 1, 2));
        UNIT_ASSERT_EQUAL(1, cache.Size());

        char buf[2] = {};
        UNIT_ASSERT(FindInCache(cache, 3, 1, buf));
        UNIT_ASSERT_STRINGS_EQUAL(buf, "d");
    }

    Y_UNIT_TEST(BetweenTwoEntries) {
        {
            TLogCache cache;
            
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", 0, 4));
            UNIT_ASSERT_EQUAL(1, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "ijkl", 8, 4));
            UNIT_ASSERT_EQUAL(2, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "efgh", 4, 4));
            UNIT_ASSERT_EQUAL(3, cache.Size());

            char buf[5] = {};
            UNIT_ASSERT(FindInCache(cache, 4, 4, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "efgh");
        }

        {
            TLogCache cache;
            
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", 0, 4));
            UNIT_ASSERT_EQUAL(1, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "ijkl", 8, 4));
            UNIT_ASSERT_EQUAL(2, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "defghi", 3, 6));
            UNIT_ASSERT_EQUAL(3, cache.Size());

            char buf[5] = {};
            UNIT_ASSERT(FindInCache(cache, 4, 4, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "efgh");
        }

        {
            TLogCache cache;
            
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", 0, 4));
            UNIT_ASSERT_EQUAL(1, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "ijkl", 8, 4));
            UNIT_ASSERT_EQUAL(2, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "efgh", 4, 4));
            UNIT_ASSERT_EQUAL(3, cache.Size());

            char buf[7] = {};
            UNIT_ASSERT(FindInCache(cache, 3, 6, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "defghi");
        }

        {
            TLogCache cache;
            
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", 0, 4));
            UNIT_ASSERT_EQUAL(1, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "ijkl", 8, 4));
            UNIT_ASSERT_EQUAL(2, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "defghi", 3, 6));
            UNIT_ASSERT_EQUAL(3, cache.Size());

            char buf[7] = {};
            UNIT_ASSERT(FindInCache(cache, 3, 6, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "defghi");
        }
    }

    Y_UNIT_TEST(NoDuplicates) {
        {
            TLogCache cache;
            
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", 0, 4));
            UNIT_ASSERT_EQUAL(1, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "def", 3, 3));
            UNIT_ASSERT_EQUAL(2, cache.Size());

            char buf[2] = {};
            UNIT_ASSERT(FindInCache(cache, 3, 1, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "d");

            char buf2[3] = {};
            UNIT_ASSERT(FindInCache(cache, 3, 2, buf2));
            UNIT_ASSERT_STRINGS_EQUAL(buf2, "de");

            char buf3[11] = {};
            UNIT_ASSERT(!FindInCache(cache, 3, 10, buf3));
            UNIT_ASSERT_STRINGS_EQUAL(buf3, "");
        }

        {
            TLogCache cache;
            
            UNIT_ASSERT(InsertIntoCache(cache, "def", 3, 3));
            UNIT_ASSERT_EQUAL(1, cache.Size());
            UNIT_ASSERT(InsertIntoCache(cache, "abcd", 0, 4));
            UNIT_ASSERT_EQUAL(2, cache.Size());

            char buf[2] = {};
            UNIT_ASSERT(FindInCache(cache, 3, 1, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "d");

            char buf2[5] = {};
            UNIT_ASSERT(FindInCache(cache, 0, 4, buf2));
            UNIT_ASSERT_STRINGS_EQUAL(buf2, "abcd");

            char buf3[11] = {};
            UNIT_ASSERT(!FindInCache(cache, 3, 10, buf3));
            UNIT_ASSERT_STRINGS_EQUAL(buf3, "");
        }

        {
            TLogCache cache;

            UNIT_ASSERT(InsertIntoCache(cache, "abcdefghij", 0, 10));
            UNIT_ASSERT_EQUAL(1, cache.Size());

            UNIT_ASSERT(InsertIntoCache(cache, "klmno", 10, 5));
            UNIT_ASSERT_EQUAL(2, cache.Size());

            UNIT_ASSERT(!InsertIntoCache(cache, "fghijklmno", 5, 10));
            
            UNIT_ASSERT_EQUAL(2, cache.Size());
        }

        {
            TLogCache cache;

            UNIT_ASSERT(InsertIntoCache(cache, "abcdefghij", 0, 10));
            UNIT_ASSERT_EQUAL(1, cache.Size());

            UNIT_ASSERT(InsertIntoCache(cache, "klmno", 10, 5));
            UNIT_ASSERT_EQUAL(2, cache.Size());

            UNIT_ASSERT(InsertIntoCache(cache, "fghijklmnopq", 5, 12));
            UNIT_ASSERT_EQUAL(3, cache.Size());

            char buf[18] = {};
            UNIT_ASSERT(FindInCache(cache, 0, 17, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "abcdefghijklmnopq");
        }

        {
            TLogCache cache;
            UNIT_ASSERT(InsertIntoCache(cache, "abcde", 0, 5)); 
            UNIT_ASSERT(InsertIntoCache(cache, "fghij", 5, 5)); 
            UNIT_ASSERT(InsertIntoCache(cache, "klmno", 10, 5));
            UNIT_ASSERT(InsertIntoCache(cache, "pqrst", 15, 5));
            
            UNIT_ASSERT(!InsertIntoCache(cache, "hijklmnopq", 7, 10));

            char buf[21] = {};
            UNIT_ASSERT(FindInCache(cache, 0, 20, buf));
            UNIT_ASSERT_STRINGS_EQUAL(buf, "abcdefghijklmnopqrst");
        }
    }

    TLogCache SetupCache(const TVector<std::pair<ui64, TString>>& content = {{5, "x"}, {1, "y"}, {10, "z"}}) {
        TLogCache cache;
        for (auto pair : content) {
            auto& data = pair.second;

            InsertIntoCache(cache, data.c_str(), pair.first, data.Size());
        }
        return cache;
    };

    void AssertCacheContains(TLogCache& cache, const TVector<std::pair<ui64, TString>>& content = {{5, "x"}, {1, "y"}, {10, "z"}}) {
        UNIT_ASSERT_VALUES_EQUAL(content.size(), cache.Size());

        char buf[2] = {};

        for (auto pair : content) {
            UNIT_ASSERT(FindInCache(cache, pair.first, 1, buf));

            UNIT_ASSERT_STRINGS_EQUAL(pair.second, buf);
        }

        for (auto pair : content) {
            cache.EraseRange(pair.first, pair.first + 1);

            UNIT_ASSERT(!FindInCache(cache, pair.first, 1, buf));
        }
    }

    Y_UNIT_TEST(EraseRangeOnEmpty) {
        TLogCache cache;
        UNIT_ASSERT_EQUAL(0, cache.EraseRange(0, 0));
        UNIT_ASSERT_EQUAL(0, cache.EraseRange(0, 10));
        UNIT_ASSERT_EQUAL(0, cache.EraseRange(10, 10));
    }

    Y_UNIT_TEST(EraseRangeOutsideOfData) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(3, cache.Size());
        UNIT_ASSERT_EQUAL(0, cache.EraseRange(0, 1));
        UNIT_ASSERT_EQUAL(3, cache.Size());
        UNIT_ASSERT_EQUAL(0, cache.EraseRange(11, 12));
        UNIT_ASSERT_EQUAL(3, cache.Size());
        UNIT_ASSERT_EQUAL(0, cache.EraseRange(11, 100));
        UNIT_ASSERT_EQUAL(3, cache.Size());
    }

    Y_UNIT_TEST(EraseRangeSingleMinElement) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(1, cache.EraseRange(1, 2));
        AssertCacheContains(cache, {{5, "x"}, {10, "z"}});
    }

    Y_UNIT_TEST(EraseRangeSingleMidElement) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(1, cache.EraseRange(5, 6));
        AssertCacheContains(cache, {{1, "y"}, {10, "z"}});
    }

    Y_UNIT_TEST(EraseRangeSingleMaxElement) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(1, cache.EraseRange(10, 11));
        AssertCacheContains(cache, {{5, "x"}, {1, "y"}});
    }

    Y_UNIT_TEST(EraseRangeSample) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(2, cache.EraseRange(2, 100));
        AssertCacheContains(cache, {{1, "y"}});
    }

    Y_UNIT_TEST(EraseRangeAllExact) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(3, cache.EraseRange(1, 11));
        UNIT_ASSERT_EQUAL(0, cache.Size());
    }

    Y_UNIT_TEST(EraseRangeAllAmple) {
        TLogCache cache = SetupCache();
        UNIT_ASSERT_EQUAL(3, cache.EraseRange(0, 100));
        UNIT_ASSERT_EQUAL(0, cache.Size());
    }
}

} // NPDisk
} // NKikimr
