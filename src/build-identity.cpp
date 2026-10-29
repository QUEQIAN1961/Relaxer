// Exported metadata survives linker garbage collection in every shipped core PE.
// Deployment compares this compiled value with the committed source policy.
#ifdef _WIN32
#define RELAXER_IDENTITY_EXPORT __declspec(dllexport)
#else
#define RELAXER_IDENTITY_EXPORT __attribute__((visibility("default")))
#endif
extern "C" RELAXER_IDENTITY_EXPORT const char RelaxerBuildSourceSHA256[] =
    "RelaxerBuildSourceSHA256=" RELAXER_BUILD_SOURCE_SHA256;
