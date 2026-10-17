// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
namespace cyinsights_quest {
// Install/remove only on a serialized RHI path. Next remains immutable once
// installed so a later interceptor can safely continue chaining through us.
template<class Function> class TimestampDispatch {
    Function next_=nullptr;
    std::atomic<bool> enabled_{false};
public:
    bool Install(Function& slot,Function wrapper) {
        if(slot==wrapper)return next_!=nullptr;
        if(!slot||next_)return false;
        next_=slot;slot=wrapper;return true;
    }
    void Remove(Function& slot,Function wrapper){Disable();if(slot==wrapper)slot=next_;}
    void SetEnabled(bool value){enabled_.store(value,std::memory_order_relaxed);}
    void Disable(){SetEnabled(false);}
    Function Next() const {return next_;}
    template<class Stage> Stage Transform(Stage stage,Stage entry,Stage completion) const {
        return enabled_.load(std::memory_order_relaxed)&&stage==entry?completion:stage;
    }
};
}
