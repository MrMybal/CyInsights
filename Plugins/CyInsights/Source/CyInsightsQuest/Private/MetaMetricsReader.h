// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <openxr/openxr.h>
#include <cmath>
#include <string>
#include <vector>

namespace cyinsights::quest {
struct Metric
{
    XrPath path = XR_NULL_PATH;
    std::string name;
};
struct Reading
{
    XrResult result = XR_ERROR_RUNTIME_FAILURE;
    XrPerformanceMetricsCounterMETA raw{XR_TYPE_PERFORMANCE_METRICS_COUNTER_META};
    bool valid_uint = false;
    bool valid_float = false;
    bool Valid() const { return valid_uint || valid_float; }
};
inline const char* UnitName(XrPerformanceMetricsCounterUnitMETA unit)
{
    switch (unit) {
    case XR_PERFORMANCE_METRICS_COUNTER_UNIT_GENERIC_META: return "generic";
    case XR_PERFORMANCE_METRICS_COUNTER_UNIT_PERCENTAGE_META: return "percent";
    case XR_PERFORMANCE_METRICS_COUNTER_UNIT_MILLISECONDS_META: return "ms";
    case XR_PERFORMANCE_METRICS_COUNTER_UNIT_BYTES_META: return "bytes";
    case XR_PERFORMANCE_METRICS_COUNTER_UNIT_HERTZ_META: return "Hz";
    default: return "unknown";
    }
}

// No clocks or inferred execution-frame identities here: the runtime supplies
// the latest counter value, not its original measurement interval.
class MetricsReader
{
public:
    bool Initialize(PFN_xrGetInstanceProcAddr get_proc, XrInstance instance)
    {
        End();
        metrics.clear();
        ready = false;
        if (!get_proc || instance == XR_NULL_HANDLE) {
            error = "OpenXR instance/loader unavailable";
            return false;
        }
        if (!Load(get_proc, instance, "xrEnumeratePerformanceMetricsCounterPathsMETA", enumerate) ||
            !Load(get_proc, instance, "xrGetPerformanceMetricsStateMETA", get_state) ||
            !Load(get_proc, instance, "xrSetPerformanceMetricsStateMETA", set_state) ||
            !Load(get_proc, instance, "xrQueryPerformanceMetricsCounterMETA", query) ||
            !Load(get_proc, instance, "xrPathToString", path_string)) {
            error = "Meta performance metrics entry point unavailable";
            return false;
        }
        uint32_t count = 0;
        if (XR_FAILED(enumerate(instance, 0, &count, nullptr)) || count > MaxCounters) {
            error = "Meta counter enumeration failed or exceeded limit";
            return false;
        }
        std::vector<XrPath> paths;
        // Retry a changing catalog, with a bounded allocation on every attempt.
        for (int retry = 0; retry < 4; ++retry) {
            if (!count || count > MaxCounters) break;
            paths.resize(count);
            const uint32_t capacity = count;
            const XrResult result = enumerate(instance, capacity, &count, paths.data());
            if (result == XR_ERROR_SIZE_INSUFFICIENT) continue;
            if (XR_FAILED(result) || count > capacity) {
                error = "Meta counter enumeration failed";
                return false;
            }
            paths.resize(count);
            for (XrPath path : paths) {
                char text[XR_MAX_PATH_LENGTH]{};
                uint32_t length = 0;
                if (XR_FAILED(path_string(instance, path, sizeof(text), &length, text)) ||
                    !length || length > sizeof(text) || text[length - 1] != '\0') {
                    // Preserve the identity even if a runtime cannot resolve its name.
                    metrics.push_back({path, "unresolved_path_" + std::to_string(path)});
                } else {
                    metrics.push_back({path, text});
                }
            }
            ready = !metrics.empty();
            error = ready ? "" : "Meta runtime exposes no performance counters";
            return ready;
        }
        error = count ? "Meta counter catalog changed repeatedly or exceeded limit" :
                        "Meta runtime exposes no performance counters";
        return false;
    }

    bool Begin(XrSession in_session)
    {
        if (session == in_session && session != XR_NULL_HANDLE) return true;
        End();
        if (!ready || in_session == XR_NULL_HANDLE) return false;
        XrPerformanceMetricsStateMETA state{XR_TYPE_PERFORMANCE_METRICS_STATE_META};
        if (XR_FAILED(get_state(in_session, &state))) {
            error = "Cannot query Meta performance metrics enable state";
            return false;
        }
        owns_enable = state.enabled == XR_FALSE;
        if (owns_enable) {
            state.enabled = XR_TRUE;
            if (XR_FAILED(set_state(in_session, &state))) {
                owns_enable = false;
                error = "Cannot enable Meta performance metrics";
                return false;
            }
        }
        session = in_session;
        error.clear();
        return true;
    }

    void End()
    {
        if (session != XR_NULL_HANDLE && owns_enable && set_state) {
            XrPerformanceMetricsStateMETA state{XR_TYPE_PERFORMANCE_METRICS_STATE_META};
            state.enabled = XR_FALSE;
            if (XR_FAILED(set_state(session, &state)))
                error = "Cannot restore Meta performance metrics enable state";
        }
        session = XR_NULL_HANDLE;
        owns_enable = false;
    }

    Reading Read(XrPath path) const
    {
        Reading value;
        if (session == XR_NULL_HANDLE || !query) return value;
        value.result = query(session, path, &value.raw);
        if (XR_SUCCEEDED(value.result)) {
            value.valid_uint = (value.raw.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_UINT_VALUE_VALID_BIT_META) != 0;
            value.valid_float = (value.raw.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_FLOAT_VALUE_VALID_BIT_META) != 0 &&
                                std::isfinite(value.raw.floatValue);
        }
        return value;
    }

    bool Ready() const { return ready; }
    bool Active() const { return session != XR_NULL_HANDLE; }
    const std::string& Error() const { return error; }
    const std::vector<Metric>& Metrics() const { return metrics; }

private:
    template<typename Function>
    static bool Load(PFN_xrGetInstanceProcAddr get_proc, XrInstance instance, const char* name, Function& out)
    {
        PFN_xrVoidFunction function = nullptr;
        const XrResult result = get_proc(instance, name, &function);
        out = reinterpret_cast<Function>(function);
        return XR_SUCCEEDED(result) && out != nullptr;
    }
    static constexpr uint32_t MaxCounters = 1024;
    bool ready = false, owns_enable = false;
    XrSession session = XR_NULL_HANDLE;
    std::string error;
    std::vector<Metric> metrics;
    PFN_xrEnumeratePerformanceMetricsCounterPathsMETA enumerate = nullptr;
    PFN_xrGetPerformanceMetricsStateMETA get_state = nullptr;
    PFN_xrSetPerformanceMetricsStateMETA set_state = nullptr;
    PFN_xrQueryPerformanceMetricsCounterMETA query = nullptr;
    PFN_xrPathToString path_string = nullptr;
};
}
