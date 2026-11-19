#include "cyui/modules/DocumentWeb.h"
namespace cyui {
bool DocumentWebAdapter::draw(const DocumentMedia &media, ImVec2 size) {
    auto *view = backend ? backend(media) : nullptr;
    if (!view)
        return false;
    auto &known = urls_[media.id];
    if (known.first != view || known.second != media.source) {
        if (!view->navigate(media.source))
            return false;
        known = {view, media.source};
    }
    ItemScope scope{media.id};
    for (const auto &value : webPanel("##documentWeb", *view, size))
        if (event)
            event(media, value);
    return true;
}
} // namespace cyui
