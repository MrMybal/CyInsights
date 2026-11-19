#pragma once
#include "cyui/modules/DocumentViews.h"
#include "cyui/modules/Web.h"
namespace cyui {
// Optional adapter: use either a CyUI CEF backend or a host's existing WebBackend.
// It never creates a second runtime. The host pumps and brackets backend frames,
// including hidden panels, and owns the backends until all callbacks are finished.
class CYUI_API DocumentWebAdapter {
  public:
    std::function<WebBackend *(const DocumentMedia &)> backend;
    std::function<void(const DocumentMedia &, const WebEvent &)> event;
    bool draw(const DocumentMedia &, ImVec2 size);
    void clear() { urls_.clear(); }

  private:
    std::unordered_map<ItemId, std::pair<WebBackend *, std::string>> urls_;
};
} // namespace cyui
