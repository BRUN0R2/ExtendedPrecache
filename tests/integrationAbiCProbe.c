#include <stddef.h>
#include <stdint.h>

#pragma pack(push, 1)
#include "integrationAbiV2.h"
struct PackSentinel {
    uint8_t first;
    uint32_t second;
};
#pragma pack(pop)

_Static_assert(sizeof(struct PackSentinel) == 5, "restore caller packing");
_Static_assert(sizeof(EpcAbiStatusV2) == 4, "status is u32, not C enum");
_Static_assert(sizeof(EpcGenerationV2) == 8, "generation words");
_Static_assert(sizeof(EpcLeaseSerialV2) == 8, "lease words");
_Static_assert(sizeof(EpcMapEpochV2) == 8, "map epoch words");
_Static_assert(sizeof(EpcMapKeyV2) == 24, "generation bound map");
_Static_assert(sizeof(EpcSnapshotHandleV2) == 24, "runtime bound lease");
_Static_assert(sizeof(EpcApiLeaseV2) == 24, "API lease has independent ownership");
_Static_assert(sizeof(EpcResourceRefV2) == 32, "map bound reference");
_Static_assert(sizeof(EpcMapInfoV2) == 104, "map output prefix");
_Static_assert(sizeof(EpcResourceRecordV2) == 52, "resource output prefix");
_Static_assert(offsetof(EpcMapInfoV2, catalogSha256) == 72, "digest offset");
_Static_assert(offsetof(EpcResourceRecordV2, categoryIndex) == 40, "index offset");

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(EpcIntegrationApiV2) == 80, "x86 function table");
_Static_assert(offsetof(EpcIntegrationApiV2, releaseApi) == 76, "last function offset");
#else
_Static_assert(sizeof(EpcIntegrationApiV2) != 80, "x64 is not the x86 ABI");
#endif
