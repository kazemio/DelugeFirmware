#include "CppUTest/TestHarness.h"
#include "display_mock.h"
#include "model/sync.h"
#include "song_mock.h"

TEST_GROUP(SyncTests){};

TEST(SyncTests, wrapSwingIntervalSyncLevel) {
	for (int32_t i = 1; i < NUM_SWING_INTERVALS; i++) {
		CHECK_EQUAL(i, wrapSwingIntervalSyncLevel(i));
	}
	CHECK_EQUAL(NUM_SWING_INTERVALS - 2, wrapSwingIntervalSyncLevel(-1));
	CHECK_EQUAL(NUM_SWING_INTERVALS - 1, wrapSwingIntervalSyncLevel(0));
	CHECK_EQUAL(1, wrapSwingIntervalSyncLevel(NUM_SWING_INTERVALS));
	CHECK_EQUAL(2, wrapSwingIntervalSyncLevel(NUM_SWING_INTERVALS + 1));
}

TEST(SyncTests, syncValueToSyncType) {
	for (int32_t i = 0; i < SYNC_TYPE_TRIPLET; i++) {
		CHECK_EQUAL(SYNC_TYPE_EVEN, syncValueToSyncType(i));
	}
	for (int32_t i = SYNC_TYPE_TRIPLET + 1; i < SYNC_TYPE_DOTTED; i++) {
		CHECK_EQUAL(SYNC_TYPE_TRIPLET, syncValueToSyncType(i));
	}
	for (int32_t i = SYNC_TYPE_DOTTED + 1; i < NUM_SYNC_VALUES; i++) {
		CHECK_EQUAL(SYNC_TYPE_DOTTED, syncValueToSyncType(i));
	}
}

TEST(SyncTests, syncTypeAndLevelToSyncValue) {
	// Every sync value decodes to a (type, level) pair that encodes back to itself
	for (int32_t i = 0; i < NUM_SYNC_VALUES; i++) {
		CHECK_EQUAL(i, syncTypeAndLevelToSyncValue(syncValueToSyncType(i), syncValueToSyncLevel(i)));
	}
}

TEST(SyncTests, lfoSyncParamValueRoundTrip) {
	for (int32_t i = 0; i < NUM_SYNC_VALUES; i++) {
		int32_t paramValue = lfoSyncValueToParamValue(i);
		// INT32_MIN is the "param never set" sentinel; no encoded value may collide with it
		CHECK(paramValue != -2147483648);
		CHECK_EQUAL(i, lfoSyncParamValueToSyncValue(paramValue));
	}
	// Full param range maps onto valid sync values at the extremes
	CHECK_EQUAL(0, lfoSyncParamValueToSyncValue(-2147483648));
	CHECK_EQUAL(NUM_SYNC_VALUES - 1, lfoSyncParamValueToSyncValue(2147483647));
}

TEST(SyncTests, syncValueToSyncLevel) {
	// Zero is off
	for (int32_t i = 0; i <= MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(i, syncValueToSyncLevel(i));
	}
	// Zero as level doesn't exist for triplets and dotted
	for (int32_t i = 0; i < MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(i + 1, syncValueToSyncLevel(i + SYNC_TYPE_TRIPLET));
	}
	for (int32_t i = 0; i < MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(i + 1, syncValueToSyncLevel(i + SYNC_TYPE_DOTTED));
	}
}
