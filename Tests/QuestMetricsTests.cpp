// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "MetaMetricsReader.h"
#include <cstring>
#include <iostream>
#include <limits>

namespace {
struct FakeRuntime
{
    uint32_t count = 2;
    bool enabled = false, missing_query = false, bad_name = false, growing_catalog = false;
    XrResult enumerate_result = XR_SUCCESS, get_result = XR_SUCCESS, set_result = XR_SUCCESS, query_result = XR_SUCCESS;
    int enables = 0, disables = 0, queries = 0;
    XrPerformanceMetricsCounterFlagsMETA flags = XR_PERFORMANCE_METRICS_COUNTER_FLOAT_VALUE_VALID_BIT_META;
    float value = 4.25f;
    uint32_t uint_value = 0;
    XrPerformanceMetricsCounterUnitMETA unit = XR_PERFORMANCE_METRICS_COUNTER_UNIT_MILLISECONDS_META;
} runtime;
XrInstance Instance() { return reinterpret_cast<XrInstance>(uintptr_t(1)); }
XrSession Session() { return reinterpret_cast<XrSession>(uintptr_t(2)); }
XrResult XRAPI_PTR Enumerate(XrInstance, uint32_t capacity, uint32_t* count, XrPath* paths)
{
    *count = runtime.count;
    if (XR_FAILED(runtime.enumerate_result)) return runtime.enumerate_result;
    if (runtime.growing_catalog && capacity) { *count = ++runtime.count; return XR_ERROR_SIZE_INSUFFICIENT; }
    if (!capacity) return XR_SUCCESS;
    if (capacity < *count) return XR_ERROR_SIZE_INSUFFICIENT;
    for (uint32_t i = 0; i < *count; ++i) paths[i] = i + 100;
    return XR_SUCCESS;
}
XrResult XRAPI_PTR GetState(XrSession, XrPerformanceMetricsStateMETA* state)
{
    state->enabled = runtime.enabled ? XR_TRUE : XR_FALSE;
    return runtime.get_result;
}
XrResult XRAPI_PTR SetState(XrSession, const XrPerformanceMetricsStateMETA* state)
{
    if (XR_FAILED(runtime.set_result)) return runtime.set_result;
    runtime.enabled = state->enabled == XR_TRUE;
    if (runtime.enabled) ++runtime.enables; else ++runtime.disables;
    return XR_SUCCESS;
}
XrResult XRAPI_PTR Query(XrSession, XrPath, XrPerformanceMetricsCounterMETA* value)
{
    ++runtime.queries;
    value->counterFlags = runtime.flags;
    value->counterUnit = runtime.unit;
    value->floatValue = runtime.value;
    value->uintValue = runtime.uint_value;
    return runtime.query_result;
}
XrResult XRAPI_PTR PathString(XrInstance, XrPath, uint32_t capacity, uint32_t* count, char* text)
{
    if (runtime.bad_name) return XR_ERROR_PATH_INVALID;
    const char* name = "/perfmetrics_meta/app/gpu_frametime";
    *count = static_cast<uint32_t>(std::strlen(name) + 1);
    if (capacity < *count) return XR_ERROR_SIZE_INSUFFICIENT;
    std::memcpy(text, name, *count);
    return XR_SUCCESS;
}
XrResult XRAPI_PTR GetProc(XrInstance, const char* name, PFN_xrVoidFunction* out)
{
    *out = nullptr;
    if (std::strcmp(name, "xrEnumeratePerformanceMetricsCounterPathsMETA") == 0) *out = reinterpret_cast<PFN_xrVoidFunction>(Enumerate);
    if (std::strcmp(name, "xrGetPerformanceMetricsStateMETA") == 0) *out = reinterpret_cast<PFN_xrVoidFunction>(GetState);
    if (std::strcmp(name, "xrSetPerformanceMetricsStateMETA") == 0) *out = reinterpret_cast<PFN_xrVoidFunction>(SetState);
    if (std::strcmp(name, "xrQueryPerformanceMetricsCounterMETA") == 0 && !runtime.missing_query) *out = reinterpret_cast<PFN_xrVoidFunction>(Query);
    if (std::strcmp(name, "xrPathToString") == 0) *out = reinterpret_cast<PFN_xrVoidFunction>(PathString);
    return *out ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
}
}

int main()
{
    int checks = 0, failures = 0;
    auto test = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    using cyinsights::quest::MetricsReader;
    MetricsReader reader;
    test(!reader.Read(100).Valid() && runtime.queries == 0, "no query without a session");
    test(!reader.Initialize(nullptr, Instance()), "missing OpenXR loader");
    test(!reader.Initialize(GetProc, XR_NULL_HANDLE), "missing OpenXR instance");
    runtime.missing_query = true;
    test(!reader.Initialize(GetProc, Instance()) && !reader.Ready(), "missing extension entry point");
    runtime = {};
    runtime.count = 0;
    test(!reader.Initialize(GetProc, Instance()), "empty catalog does not fabricate GPU values");
    runtime.count = 1025;
    test(!reader.Initialize(GetProc, Instance()), "bound malicious/invalid runtime catalog");
    runtime = {};
    runtime.enumerate_result = XR_ERROR_RUNTIME_FAILURE;
    test(!reader.Initialize(GetProc, Instance()), "catalog failure");
    runtime = {};
    runtime.growing_catalog = true;
    test(!reader.Initialize(GetProc, Instance()), "bounded changing-catalog retries");
    runtime = {};
    runtime.bad_name = true;
    test(reader.Initialize(GetProc, Instance()) && reader.Metrics()[0].name == "unresolved_path_100", "retain unresolved counter identity");
    runtime = {};
    test(reader.Initialize(GetProc, Instance()) && reader.Metrics().size() == 2 &&
         reader.Metrics()[0].name == "/perfmetrics_meta/app/gpu_frametime", "enumerate actual runtime paths");
    test(!reader.Begin(XR_NULL_HANDLE), "no metrics activation before XR session");
    runtime.get_result = XR_ERROR_RUNTIME_FAILURE;
    test(!reader.Begin(Session()) && runtime.enables == 0, "state failure has no side effect");
    runtime.get_result = XR_SUCCESS;
    runtime.set_result = XR_ERROR_RUNTIME_FAILURE;
    test(!reader.Begin(Session()) && !reader.Active(), "enable failure keeps collector inactive");
    runtime.set_result = XR_SUCCESS;
    test(reader.Begin(Session()) && runtime.enabled && runtime.enables == 1, "enable metrics for capture");
    test(reader.Begin(Session()) && runtime.enables == 1, "repeated poll does not re-enable session");
    const auto measured = reader.Read(100);
    test(measured.valid_float && !measured.valid_uint && measured.raw.floatValue == 4.25f &&
         measured.raw.counterUnit == XR_PERFORMANCE_METRICS_COUNTER_UNIT_MILLISECONDS_META, "retain measured value and original unit");
    runtime.value = 0;
    test(reader.Read(100).Valid(), "a valid measured zero remains valid");
    runtime.flags = 0;
    test(!reader.Read(100).Valid(), "missing value is not measured zero");
    runtime.flags = XR_PERFORMANCE_METRICS_COUNTER_ANY_VALUE_VALID_BIT_META;
    test(!reader.Read(100).Valid(), "generic validity flag alone does not identify a value type");
    runtime.flags = XR_PERFORMANCE_METRICS_COUNTER_FLOAT_VALUE_VALID_BIT_META;
    runtime.value = std::numeric_limits<float>::quiet_NaN();
    test(!reader.Read(100).Valid(), "reject NaN for native counter");
    runtime.value = std::numeric_limits<float>::infinity();
    test(!reader.Read(100).Valid(), "reject infinite value");
    runtime.flags |= XR_PERFORMANCE_METRICS_COUNTER_UINT_VALUE_VALID_BIT_META;
    runtime.uint_value = 4294967295u;
    test(reader.Read(100).valid_uint && !reader.Read(100).valid_float &&
         reader.Read(100).raw.uintValue == 4294967295u, "integer validity independent of invalid float");
    runtime.value = 12;
    test(reader.Read(100).valid_uint && reader.Read(100).valid_float, "preserve both valid representations");
    runtime.query_result = XR_ERROR_RUNTIME_FAILURE;
    test(!reader.Read(100).Valid() && reader.Read(100).result == XR_ERROR_RUNTIME_FAILURE, "failed query ignores populated output and preserves error");
    runtime.query_result = XR_SUCCESS;
    runtime.unit = static_cast<XrPerformanceMetricsCounterUnitMETA>(123);
    test(reader.Read(100).Valid() && std::strcmp(cyinsights::quest::UnitName(runtime.unit), "unknown") == 0, "unknown units are preserved without guessed conversion");
    reader.End();
    test(!runtime.enabled && runtime.disables == 1 && !reader.Active(), "restore initially disabled metrics after capture");
    reader.End();
    test(runtime.disables == 1, "stop is idempotent");
    const int queries_before = runtime.queries;
    test(!reader.Read(100).Valid() && runtime.queries == queries_before, "no stale XR session read after stop");
    test(reader.Begin(Session()) && runtime.enables == 2, "next capture re-enables same live session");
    reader.End();
    runtime.enabled = true;
    const int disables_before = runtime.disables;
    test(reader.Begin(Session()), "accept externally enabled metrics");
    reader.End();
    test(runtime.enabled && runtime.disables == disables_before, "leave externally enabled metrics enabled");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
