#pragma once

#include <stdint.h>

/* Specification artifact only: no export or engine implementation is provided. */
#if defined(_WIN32)
#define epcIntegrationCall __cdecl
#else
#define epcIntegrationCall
#endif

#if defined(__cplusplus)
extern "C" {
#endif

enum { epcIntegrationMajor = 2, epcIntegrationMinor = 0, epcIntegrationPointerBits = 32 };

typedef uint32_t EpcAbiStatusV2;
enum {
    epcAbiSuccess = 0,
    epcAbiInvalidArgument = 1,
    epcAbiUnsupportedAbi = 2,
    epcAbiUnsupportedArchitecture = 3,
    epcAbiOutputTooSmall = 4,
    epcAbiNotReady = 5,
    epcAbiUnsupportedProfile = 6,
    epcAbiWrongThread = 7,
    epcAbiWrongPhase = 8,
    epcAbiStaleHandle = 9,
    epcAbiStaleMap = 10,
    epcAbiWrongKind = 11,
    epcAbiOutOfRange = 12,
    epcAbiNotFound = 13,
    epcAbiBusy = 14,
    epcAbiBudgetExceeded = 15,
    epcAbiInternalFailure = 16
};

enum { epcKindModel = 1, epcKindSound = 2, epcKindGeneric = 3, epcKindEvent = 4, epcKindDecal = 5 };

enum {
    epcMapEmpty = 0,
    epcMapBuilding = 1,
    epcMapFrozen = 2,
    epcMapLoading = 3,
    epcMapActive = 4,
    epcMapRetiring = 5,
    epcMapRetired = 6,
    epcMapFailed = 7
};

enum { epcRoleServer = 1, epcRoleClient = 2 };

/* Preserve the caller's packing; never pack the native engine's structures. */
#pragma pack(push, 4)

typedef struct EpcRuntimeNonceV2 {
    uint8_t bytes[16];
} EpcRuntimeNonceV2;

typedef struct EpcGenerationV2 {
    uint32_t low;
    uint32_t high;
} EpcGenerationV2;

typedef struct EpcLeaseSerialV2 {
    uint32_t low;
    uint32_t high;
} EpcLeaseSerialV2;

typedef struct EpcMapEpochV2 {
    uint32_t low;
    uint32_t high;
} EpcMapEpochV2;

typedef struct EpcMapKeyV2 {
    EpcRuntimeNonceV2 runtime;
    EpcGenerationV2 generation;
} EpcMapKeyV2;

typedef struct EpcSnapshotHandleV2 {
    EpcRuntimeNonceV2 runtime;
    EpcLeaseSerialV2 serial;
} EpcSnapshotHandleV2;

typedef struct EpcApiLeaseV2 {
    EpcRuntimeNonceV2 runtime;
    EpcLeaseSerialV2 serial;
} EpcApiLeaseV2;

typedef struct EpcResourceRefV2 {
    EpcMapKeyV2 map;
    uint32_t kind;
    uint32_t domainId;
} EpcResourceRefV2;

typedef struct EpcMapInfoV2 {
    uint32_t structSize;
    EpcMapKeyV2 map;
    uint32_t phase;
    uint32_t modelCount;
    uint32_t soundCount;
    uint32_t genericCount;
    uint32_t eventCount;
    uint32_t decalCount;
    uint32_t resourceCount;
    EpcMapEpochV2 mapEpoch;
    uint32_t spawnCount;
    uint32_t reserved;
    uint8_t catalogSha256[32];
} EpcMapInfoV2;

typedef struct EpcResourceRecordV2 {
    uint32_t structSize;
    EpcResourceRefV2 resource;
    uint32_t resourceOrdinal;
    uint32_t categoryIndex;
    uint32_t nameBytes;
    uint32_t reserved;
} EpcResourceRecordV2;

typedef EpcAbiStatusV2(epcIntegrationCall *EpcGetCurrentMapV2)(void *context,
                                                               const EpcApiLeaseV2 *api,
                                                               uint32_t outputBytes,
                                                               EpcMapInfoV2 *output);
typedef EpcAbiStatusV2(epcIntegrationCall *EpcAcquireSnapshotV2)(void *context,
                                                                 const EpcApiLeaseV2 *api,
                                                                 const EpcMapKeyV2 *expectedMap,
                                                                 EpcSnapshotHandleV2 *output);
typedef EpcAbiStatusV2(epcIntegrationCall *EpcFindResourceV2)(
    void *context, const EpcApiLeaseV2 *api, const EpcSnapshotHandleV2 *snapshot, uint32_t kind,
    const char *name, uint32_t nameBytes, EpcResourceRefV2 *output);
typedef EpcAbiStatusV2(epcIntegrationCall *EpcReadResourceV2)(
    void *context, const EpcApiLeaseV2 *api, const EpcSnapshotHandleV2 *snapshot,
    const EpcResourceRefV2 *resource, uint32_t outputBytes, EpcResourceRecordV2 *output);
typedef EpcAbiStatusV2(epcIntegrationCall *EpcCopyResourceNameV2)(
    void *context, const EpcApiLeaseV2 *api, const EpcSnapshotHandleV2 *snapshot,
    const EpcResourceRefV2 *resource, char *output, uint32_t outputBytes);
typedef EpcAbiStatusV2(epcIntegrationCall *EpcReleaseSnapshotV2)(void *context,
                                                                 const EpcApiLeaseV2 *api,
                                                                 EpcSnapshotHandleV2 *snapshot);
typedef EpcAbiStatusV2(epcIntegrationCall *EpcReleaseApiV2)(void *context, EpcApiLeaseV2 *api);

typedef struct EpcIntegrationApiV2 {
    uint32_t structSize;
    uint32_t abiMajor;
    uint32_t abiMinor;
    uint32_t pointerBits;
    uint32_t role;
    uint32_t reserved;
    void *context;
    EpcApiLeaseV2 lease;
    EpcGetCurrentMapV2 getCurrentMap;
    EpcAcquireSnapshotV2 acquireSnapshot;
    EpcFindResourceV2 findResource;
    EpcReadResourceV2 readResource;
    EpcCopyResourceNameV2 copyResourceName;
    EpcReleaseSnapshotV2 releaseSnapshot;
    EpcReleaseApiV2 releaseApi;
} EpcIntegrationApiV2;

typedef EpcAbiStatusV2(epcIntegrationCall *EpcQueryIntegrationApiV2)(uint32_t requestedMajor,
                                                                     uint32_t requestedMinor,
                                                                     uint32_t outputBytes,
                                                                     EpcIntegrationApiV2 *output);

#pragma pack(pop)

#if defined(__cplusplus)
}
#endif

#undef epcIntegrationCall
