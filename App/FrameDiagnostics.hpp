// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <array>
#include <atomic>
#include <string>
#include <vector>
#include <utility>
namespace cyinsights {
enum class FrameDomain { Other=-1, Game=0, Render=1, Gpu=2 };
enum class FrameCause { WithinBudget, Game, Render, Gpu, Mixed, Wait, Unknown };
struct FrameWork {
    double total=0,wait=0,active=0;
    bool available=false,incomplete=false;
};
struct FrameDiagnostic {std::array<FrameWork,3> work{};};
FrameDomain DiagnosticDomain(const std::string& thread);
bool DiagnosticWait(const std::string& name);
FrameCause DiagnoseFrame(const FrameDiagnostic& frame,double duration,double budget);
const char* FrameCauseName(FrameCause cause);
// Streaming unions retain compact coverage, never inclusive sums of nested scopes.
// One instance per native track: concurrent GPU queues/duplicate named threads
// are compared separately rather than added as if they executed serially.
class FrameCoverage {
public:
    explicit FrameCoverage(FrameDomain domain=FrameDomain::Other):domain(domain){}
    void Add(double start,double end,bool wait,bool unfinished,int depth=-1);
    void Finish();
    FrameWork Query(double start,double end) const;
    FrameDomain domain;
private:
    struct Union {
        std::vector<std::pair<double,double>> intervals;
        std::vector<double> prefix;
        bool ordered=true;double last=-1e300;
        void Add(double a,double b);
        void Finish();
        double Before(double t) const;
        double Query(double a,double b) const;
    };
    Union total_,wait_,resolved_wait_;
    std::vector<double> unfinished_;
    struct Scope {double end;int depth;bool wait;};
    std::vector<Scope> stack_;
    double cursor_=0,last_start_=-1e300;
    bool nesting_=true,has_depth_=false;
    void Advance(double time);
};
std::vector<FrameDiagnostic> BuildFrameDiagnostics(std::vector<FrameCoverage>& coverage,
    const std::vector<std::pair<double,double>>& frames,const std::atomic<bool>* cancel=nullptr);
}
