// test/core/prefetcher/Prefetcher_test.cpp
#include "NextLinePrefetchEngine.hpp"
#include "StridePrefetchEngine.hpp"
#include "Prefetcher.hpp"

#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"

#include <iostream>
#include <string>

using namespace std;

/*!
 * \brief Unit tests for the prefetch engines (NextLine and Stride).
 *
 * These tests exercise the engine logic directly, without requiring
 * a full Sparta simulation tree.
 */

// Helper to create a MemoryAccessInfoPtr from an address
static olympia::MemoryAccessInfoPtr makeAccess(uint64_t addr)
{
    return sparta::SpartaSharedPointer<olympia::MemoryAccessInfo>(
        new olympia::MemoryAccessInfo(addr));
}

void testNextLinePrefetcher()
{
    cout << "Testing NextLinePrefetchEngine..." << endl;

    olympia::NextLinePrefetchEngine engine(2, 64);

    auto access = makeAccess(0x1000);

    // Initially no prefetches should be ready
    EXPECT_FALSE(engine.isPrefetchReady());

    // Pass access into engine
    bool ret = engine.handleMemoryAccess(access);
    EXPECT_TRUE(ret);

    // Prefetches should now be ready
    EXPECT_TRUE(engine.isPrefetchReady());

    // Pop first prefetch
    auto prefetch1 = engine.getPrefetchMemoryAccess();
    EXPECT_TRUE(prefetch1 != nullptr);
    // Verify the prefetch address is the next cache line
    EXPECT_EQUAL(prefetch1->getVAddr(), static_cast<sparta::memory::addr_t>(0x1040));
    engine.popPrefetchMemoryAccess();

    // Pop second prefetch (if present)
    EXPECT_TRUE(engine.isPrefetchReady());
    auto prefetch2 = engine.getPrefetchMemoryAccess();
    EXPECT_TRUE(prefetch2 != nullptr);
    EXPECT_EQUAL(prefetch2->getVAddr(), static_cast<sparta::memory::addr_t>(0x1080));
    engine.popPrefetchMemoryAccess();

    EXPECT_FALSE(engine.isPrefetchReady());

    cout << "NextLinePrefetchEngine tests: PASSED" << endl;
}

void testStridePrefetcher()
{
    cout << "Testing StridePrefetchEngine..." << endl;

    // confidence_threshold = 1 means after 2 accesses with same stride, prefetch on 3rd
    olympia::StridePrefetchEngine engine(2, 64, 256, 1);

    auto a1 = makeAccess(0x1000);
    auto a2 = makeAccess(0x1100);
    auto a3 = makeAccess(0x1200);

    // First access — no stride yet
    engine.handleMemoryAccess(a1);
    EXPECT_FALSE(engine.isPrefetchReady());

    // Second access — stride detected but confidence not yet met
    engine.handleMemoryAccess(a2);
    EXPECT_FALSE(engine.isPrefetchReady());

    // Third access — stride confirmed, prefetches generated
    bool ret = engine.handleMemoryAccess(a3);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(engine.isPrefetchReady());

    // Consume all prefetches
    uint32_t prefetch_count = 0;
    while (engine.isPrefetchReady())
    {
        auto p = engine.getPrefetchMemoryAccess();
        EXPECT_TRUE(p != nullptr);
        engine.popPrefetchMemoryAccess();
        prefetch_count++;
    }
    EXPECT_EQUAL(prefetch_count, static_cast<uint32_t>(2));

    EXPECT_FALSE(engine.isPrefetchReady());
    cout << "StridePrefetchEngine tests: PASSED" << endl;
}

void testEdgeCases()
{
    cout << "Testing edge cases..." << endl;

    // Single prefetch next-line engine
    {
        olympia::NextLinePrefetchEngine e1(1, 64);
        auto a = makeAccess(0x1000);
        e1.handleMemoryAccess(a);
        EXPECT_TRUE(e1.isPrefetchReady());
        e1.popPrefetchMemoryAccess();
        EXPECT_FALSE(e1.isPrefetchReady());
    }

    // Stride engine with zero stride (same address repeated) — should NOT generate prefetches
    {
        olympia::StridePrefetchEngine e2(2, 64, 256, 2);
        auto b1 = makeAccess(0x2000);
        auto b2 = makeAccess(0x2000);
        auto b3 = makeAccess(0x2000);
        e2.handleMemoryAccess(b1);
        e2.handleMemoryAccess(b2);
        e2.handleMemoryAccess(b3);
        EXPECT_FALSE(e2.isPrefetchReady());
    }

    // NextLine engine: verify prefetches are cleared on new access
    {
        olympia::NextLinePrefetchEngine e3(2, 64);
        auto a1 = makeAccess(0x3000);
        e3.handleMemoryAccess(a1);
        EXPECT_TRUE(e3.isPrefetchReady());

        // New access should clear old prefetches and generate new ones
        auto a2 = makeAccess(0x4000);
        e3.handleMemoryAccess(a2);
        EXPECT_TRUE(e3.isPrefetchReady());

        auto p = e3.getPrefetchMemoryAccess();
        // Should be based on 0x4000, not 0x3000
        EXPECT_EQUAL(p->getVAddr(), static_cast<sparta::memory::addr_t>(0x4040));
        e3.popPrefetchMemoryAccess();
    }

    cout << "Edge cases tests: PASSED" << endl;
}

int main(int argc, char** argv)
{
    std::string testname = "all";

    // Parse --testname argument
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--testname" && i + 1 < argc)
        {
            testname = argv[i + 1];
            break;
        }
    }

    if (testname == "all" || testname == "nextline")
    {
        testNextLinePrefetcher();
    }
    if (testname == "all" || testname == "stride")
    {
        testStridePrefetcher();
    }
    if (testname == "all" || testname == "edge_cases")
    {
        testEdgeCases();
    }

    cout << "\nALL PREFETCHER TESTS PASSED\n" << endl;

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
