#include "cyui/modules/Nodes.h"
#include "ModuleInternal.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace cyui {
std::vector<NodeMove> arrangeNodes(const std::vector<Node> &nodes, const std::vector<NodeLink> &links,
                                   Point gap) {
    std::unordered_map<ItemId, ItemId> owners;
    std::unordered_map<ItemId, int> indegree, rank, rows;
    std::unordered_map<ItemId, std::vector<ItemId>> next;
    for (const auto &n : nodes) {
        indegree[n.id] = 0;
        for (const auto &p : n.inputs)
            owners[p.id] = n.id;
        for (const auto &p : n.outputs)
            owners[p.id] = n.id;
    }
    for (const auto &l : links)
        if (owners.count(l.from) && owners.count(l.to)) {
            const auto a = owners[l.from], b = owners[l.to];
            if (a != b) {
                next[a].push_back(b);
                ++indegree[b];
            }
        }
    std::vector<ItemId> queue;
    std::unordered_set<ItemId> placed;
    std::size_t cursor = 0;
    auto place = [&](ItemId id) {
        if (placed.insert(id).second)
            queue.push_back(id);
    };
    auto drain = [&] {
        for (; cursor < queue.size(); ++cursor)
            for (auto b : next[queue[cursor]]) {
                if (placed.count(b))
                    continue; // a back edge into a broken cycle
                rank[b] = std::max(rank[b], rank[queue[cursor]] + 1);
                if (--indegree[b] == 0)
                    place(b);
            }
    };
    for (const auto &n : nodes)
        if (!indegree[n.id])
            place(n.id);
    drain();
    // Nodes left over sit in or after a cycle: place the first, ignoring its remaining inputs.
    for (const auto &n : nodes)
        if (!placed.count(n.id)) {
            place(n.id);
            drain();
        }
    std::vector<NodeMove> result;
    for (const auto &n : nodes) {
        const int column = rank[n.id];
        result.push_back({n.id, {column * gap.x, rows[ItemId(column)]++ * gap.y}});
    }
    return result;
}
NodeResult nodeEditor(const char *id, NodeState &s, const std::vector<Node> &nodes,
                      const std::vector<NodeLink> &links, ImVec2 size, const NodeOptions &o) {
    NodeResult result;
    ScopedID viewScope(id);
    CanvasScope scope(id, s.canvas, size);
    const auto f = scope.frame();
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    auto clearSelection = [&] {
        s.selection.clear();
        s.selectedGroups.clear();
        s.selectedLinks.clear();
        s.selectedReroutes.clear();
    };
    auto pick = [&](std::vector<ItemId> &values, ItemId key, bool additive) {
        if (!selected(values, key) || io.KeyCtrl) {
            if (!additive)
                clearSelection();
            selectItem(values, key, additive);
        }
    };
    auto selection = [&] {
        return NodeSelection{s.selection, s.selectedGroups, s.selectedLinks, s.selectedReroutes};
    };
    auto hasSelection = [&] {
        return !s.selection.empty() || !s.selectedGroups.empty() || !s.selectedLinks.empty() ||
               !s.selectedReroutes.empty();
    };
    int clipboardAction = 0; // copy / paste / duplicate, shared by menus and keyboard
    bool pasteAtContext = false;
    bool contextHovered = false;
    if (s.overlayMode && (s.revision != o.revision || o.readOnly || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        if (s.overlayMode == 3)
            result.reroutes.phase = EditPhase::cancel;
        else {
            result.groups.phase = EditPhase::cancel;
            if (s.overlayMode == 1 && o.moveGroupContents) {
                result.moved.phase = EditPhase::cancel;
                result.reroutes.phase = EditPhase::cancel;
            }
        }
        s.overlayMode = 0;
        s.groupPreview.clear();
        s.routePreview.clear();
        s.groupNodesPreview.clear();
        s.groupRoutesPreview.clear();
    }
    const auto groups =
        s.overlayMode == 1 || s.overlayMode == 2 ? ArrayView<NodeGroup>(s.groupPreview) : o.groups;
    const auto routes = s.overlayMode == 3 ? ArrayView<NodeReroute>(s.routePreview)
                                           : (s.overlayMode == 1 && o.moveGroupContents
                                                  ? ArrayView<NodeReroute>(s.groupRoutesPreview)
                                                  : o.reroutes);
    const NodeGroup *hitGroup = nullptr;
    bool groupResize = false;
    const NodeReroute *hitRoute = nullptr;
    for (std::size_t i = 0; i < groups.count; ++i) {
        const auto &g = groups[i];
        auto a = s.canvas.view.screen(g.position, f.bounds.min),
             b = s.canvas.view.screen({g.position.x + g.size.x, g.position.y + g.size.y}, f.bounds.min);
        d->AddRectFilled(a, b, alpha(categorical(g.colorIndex), .09f), dp(4));
        d->AddRect(a, b, selected(s.selectedGroups, g.id) ? palette().accent : palette().lineStrong, dp(4), 0,
                   dp(1));
        clippedText(d, {a.x + dp(8), a.y + dp(4)}, b.x - a.x - dp(16), palette().textDim, g.label.c_str());
        const Rect title{a, {b.x, a.y + ImGui::GetFrameHeight()}}, corner{{b.x - dp(12), b.y - dp(12)}, b};
        if (title.contains(io.MousePos) || corner.contains(io.MousePos)) {
            hitGroup = &g;
            groupResize = corner.contains(io.MousePos);
        }
        d->AddLine({b.x - dp(10), b.y}, {b.x, b.y - dp(10)}, palette().lineStrong, dp(1));
    }
    std::unordered_map<ItemId, std::vector<Point>> routing;
    for (std::size_t i = 0; i < routes.count; ++i) {
        routing[routes[i].link].push_back(routes[i].position);
        auto p = s.canvas.view.screen(routes[i].position, f.bounds.min);
        if (std::hypot(io.MousePos.x - p.x, io.MousePos.y - p.y) < dp(9))
            hitRoute = &routes[i];
    }
    auto allocate = [&]() -> ItemId {
        if (!o.allocateId)
            return 0;
        auto fresh = o.allocateId();
        bool used = !fresh;
        for (const auto &n : nodes) {
            used |= n.id == fresh;
            for (const auto &p : n.inputs)
                used |= p.id == fresh;
            for (const auto &p : n.outputs)
                used |= p.id == fresh;
        }
        for (const auto &l : links)
            used |= l.id == fresh;
        for (std::size_t i = 0; i < o.groups.count; ++i)
            used |= o.groups[i].id == fresh;
        for (std::size_t i = 0; i < o.reroutes.count; ++i)
            used |= o.reroutes[i].id == fresh;
        if (used) {
            s.clipboardError = "Allocator returned an existing or zero ID";
            return 0;
        }
        return fresh;
    };
    const float z = float(s.canvas.view.zoom), w = dp(o.width) * z, header = dp(o.header) * z,
                row = dp(o.row) * z;
    struct Card {
        const Node *n;
        Rect rect;
    };
    struct Pin {
        ItemId id, node;
        ImVec2 p;
        bool output;
    };
    std::vector<Card> cards;
    std::vector<Pin> pins;
    std::unordered_map<ItemId, std::size_t> pinIndex;
    std::unordered_map<ItemId, Point> moved;
    const std::unordered_set<ItemId> chosen(s.selection.begin(), s.selection.end());
    auto clear = [&] {
        s.dragging = s.boxing = s.wiring = false;
        s.wire = 0;
        s.preview.clear();
        s.before.clear();
    };
    if ((s.dragging || s.wiring) &&
        (s.revision != o.revision || o.readOnly || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        clear();
        result.moved.phase = EditPhase::cancel;
    }
    if (s.dragging)
        for (const auto &p : s.preview)
            moved[p.id] = p.position;
    if (s.overlayMode == 1 && o.moveGroupContents)
        for (const auto &p : s.groupNodesPreview)
            moved[p.id] = p.position;
    cards.reserve(nodes.size());
    for (const auto &n : nodes) {
        const auto found = moved.find(n.id);
        const Point pos = found != moved.end() ? found->second : n.position;
        const auto a = s.canvas.view.screen(pos, f.bounds.min);
        const float h =
            header + row * std::max<std::size_t>(1, std::max(n.inputs.size(), n.outputs.size())) + dp(8) * z;
        cards.push_back({&n, {a, {a.x + w, a.y + h}}});
        for (std::size_t i = 0; i < n.inputs.size(); ++i)
            pins.push_back({n.inputs[i].id, n.id, {a.x, a.y + header + row * (float(i) + .5f)}, false});
        for (std::size_t i = 0; i < n.outputs.size(); ++i)
            pins.push_back({n.outputs[i].id, n.id, {a.x + w, a.y + header + row * (float(i) + .5f)}, true});
    }
    for (std::size_t i = 0; i < pins.size(); ++i)
        pinIndex.emplace(pins[i].id, i);
    auto pinAt = [&](ItemId id) -> const Pin * {
        const auto it = pinIndex.find(id);
        return it != pinIndex.end() ? &pins[it->second] : nullptr;
    };
    auto wire = [&](ImVec2 a, ImVec2 b, ImU32 color) {
        const float bend = std::max(dp(35) * z, std::abs(b.x - a.x) * .5f);
        d->AddBezierCubic(a, {a.x + bend, a.y}, {b.x - bend, b.y}, b, color, dp(2));
    };
    const NodeLink *hitLink = nullptr;
    float linkDistance = dp(7);
    for (const auto &l : links) {
        const auto *a = pinAt(l.from), *b = pinAt(l.to);
        if (!a || !b)
            continue;
        if (auto routed = routing.find(l.id); routed != routing.end()) {
            ImVec2 previous = a->p;
            auto segment = [&](ImVec2 end) {
                d->AddLine(previous, end,
                           selected(s.selectedLinks, l.id) ? palette().accent : categorical(l.colorIndex),
                           dp(2));
                const auto dx = end.x - previous.x, dy = end.y - previous.y;
                const auto k =
                    std::clamp(((io.MousePos.x - previous.x) * dx + (io.MousePos.y - previous.y) * dy) /
                                   std::max(1e-6f, dx * dx + dy * dy),
                               0.f, 1.f);
                const auto distance =
                    std::hypot(io.MousePos.x - previous.x - k * dx, io.MousePos.y - previous.y - k * dy);
                if (f.hovered && distance < linkDistance) {
                    linkDistance = distance;
                    hitLink = &l;
                }
                previous = end;
            };
            for (auto p : routed->second)
                segment(s.canvas.view.screen(p, f.bounds.min));
            segment(b->p);
            continue;
        }
        const float bend = std::max(dp(35) * z, std::abs(b->p.x - a->p.x) * .5f);
        // The curve stays inside its control points' box: skip links far from the canvas or mouse.
        const Rect box{{std::min(a->p.x, b->p.x - bend), std::min(a->p.y, b->p.y)},
                       {std::max(a->p.x + bend, b->p.x), std::max(a->p.y, b->p.y)}};
        if (box.max.x < f.bounds.min.x || box.min.x > f.bounds.max.x || box.max.y < f.bounds.min.y ||
            box.min.y > f.bounds.max.y)
            continue;
        wire(a->p, b->p, selected(s.selectedLinks, l.id) ? palette().accent : categorical(l.colorIndex));
        if (!f.hovered || io.MousePos.x < box.min.x - linkDistance ||
            io.MousePos.x > box.max.x + linkDistance || io.MousePos.y < box.min.y - linkDistance ||
            io.MousePos.y > box.max.y + linkDistance)
            continue;
        ImVec2 previous = a->p;
        for (int sample = 1; sample <= 32; ++sample) {
            const float t = sample / 32.f, u = 1 - t;
            const ImVec2 q{u * u * u * a->p.x + 3 * u * u * t * (a->p.x + bend) +
                               3 * u * t * t * (b->p.x - bend) + t * t * t * b->p.x,
                           u * u * u * a->p.y + 3 * u * u * t * a->p.y + 3 * u * t * t * b->p.y +
                               t * t * t * b->p.y};
            const float dx = q.x - previous.x, dy = q.y - previous.y,
                        k = std::clamp(
                            ((io.MousePos.x - previous.x) * dx + (io.MousePos.y - previous.y) * dy) /
                                std::max(1e-6f, dx * dx + dy * dy),
                            0.f, 1.f);
            const float distance =
                std::hypot(io.MousePos.x - previous.x - k * dx, io.MousePos.y - previous.y - k * dy);
            if (distance < linkDistance) {
                linkDistance = distance;
                hitLink = &l;
            }
            previous = q;
        }
    }
    const Node *hitNode = nullptr;
    const Pin *hitPin = nullptr;
    for (const auto &c : cards) {
        if (c.rect.contains(io.MousePos))
            hitNode = c.n;
        if (c.rect.max.x < f.bounds.min.x || c.rect.min.x > f.bounds.max.x || c.rect.max.y < f.bounds.min.y ||
            c.rect.min.y > f.bounds.max.y)
            continue;
        d->AddRectFilled(c.rect.min, c.rect.max, palette().bg2, dp(4) * z);
        d->AddRectFilled(c.rect.min, {c.rect.max.x, c.rect.min.y + header},
                         mix(palette().bg2, categorical(c.n->colorIndex), .35f), dp(4) * z);
        const bool picked = chosen.count(c.n->id) != 0;
        d->AddRect(c.rect.min, c.rect.max, picked ? palette().accent : palette().line, dp(4) * z, 0,
                   dp(picked ? 2.f : 1.f));
        clippedText(d, {c.rect.min.x + dp(8) * z, c.rect.min.y + dp(5) * z}, w - dp(16) * z, palette().text,
                    c.n->label.c_str());
        for (std::size_t i = 0; i < c.n->inputs.size(); ++i)
            clippedText(d, {c.rect.min.x + dp(10) * z, c.rect.min.y + header + row * float(i)}, w * .44f,
                        palette().textDim, c.n->inputs[i].label.c_str());
        for (std::size_t i = 0; i < c.n->outputs.size(); ++i)
            clippedText(d, {c.rect.min.x + w * .54f, c.rect.min.y + header + row * float(i)}, w * .42f,
                        palette().textDim, c.n->outputs[i].label.c_str());
        if (o.drawBody)
            o.drawBody(*c.n, {{c.rect.min.x + dp(8) * z, c.rect.min.y + header}, c.rect.max});
    }
    for (const auto &p : pins) {
        d->AddCircleFilled(p.p, dp(4) * std::max(.6f, z), palette().info);
        if (std::hypot(p.p.x - io.MousePos.x, p.p.y - io.MousePos.y) < dp(9))
            hitPin = &p;
    }
    for (std::size_t i = 0; i < routes.count; ++i) {
        auto p = s.canvas.view.screen(routes[i].position, f.bounds.min);
        d->AddCircleFilled(p, dp(5),
                           selected(s.selectedReroutes, routes[i].id) ? palette().accent : palette().bg3);
        d->AddCircle(p, dp(5), palette().accent, 0, dp(1));
    }
    if (f.hovered && ImGui::IsMouseClicked(1)) {
        s.focused = true;
        s.contextKind = hitRoute ? 3 : hitNode || hitPin ? 4 : hitGroup ? 2 : hitLink ? 1 : 0;
        s.contextId = hitRoute   ? hitRoute->id
                      : hitPin   ? hitPin->node
                      : hitNode  ? hitNode->id
                      : hitGroup ? hitGroup->id
                      : hitLink  ? hitLink->id
                                 : 0;
        if (s.contextKind) {
            auto &picked = s.contextKind == 1   ? s.selectedLinks
                           : s.contextKind == 2 ? s.selectedGroups
                           : s.contextKind == 3 ? s.selectedReroutes
                                                : s.selection;
            if (!selected(picked, s.contextId)) {
                clearSelection();
                picked.push_back(s.contextId);
            }
        }
        s.contextPosition = f.mouse;
        if (hitGroup)
            s.groupLabel = hitGroup->label;
        ImGui::OpenPopup("##nodeMenu");
    }
    if (!o.readOnly && f.hovered && hitLink && !hitNode && !hitPin && !hitRoute &&
        ImGui::IsMouseDoubleClicked(0)) {
        auto fresh = allocate();
        if (fresh)
            result.created.reroutes.push_back({fresh, hitLink->id, f.mouse});
    }
    if (ImGui::BeginPopup("##nodeMenu")) {
        contextHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (ImGui::MenuItem(trLabel("Copy").c_str(), "Ctrl+C", false, hasSelection()))
            clipboardAction = 1;
        if (ImGui::MenuItem(trLabel("Paste").c_str(), "Ctrl+V", false, !o.readOnly && bool(o.allocateId))) {
            clipboardAction = 2;
            pasteAtContext = true;
        }
        if (ImGui::MenuItem(trLabel("Duplicate").c_str(), "Ctrl+D", false,
                            hasSelection() && !o.readOnly && bool(o.allocateId)))
            clipboardAction = 3;
        ImGui::Separator();
        if (ImGui::MenuItem(trLabel("Add comment group").c_str(), nullptr, false,
                            !o.readOnly && bool(o.allocateId))) {
            auto fresh = allocate();
            if (fresh)
                result.created.groups.push_back({fresh, s.contextPosition, {400, 240}, tr("Comment"), 0});
        }
        if (s.contextKind == 1) {
            if (ImGui::MenuItem(trLabel("Add reroute").c_str(), nullptr, false,
                                !o.readOnly && bool(o.allocateId))) {
                auto fresh = allocate();
                if (fresh)
                    result.created.reroutes.push_back({fresh, s.contextId, s.contextPosition});
            }
            if (ImGui::MenuItem(trLabel("Remove link").c_str(), nullptr, false, !o.readOnly)) {
                result.removeLink = s.contextId;
                result.linkRemoved = true;
            }
        }
        if (s.contextKind == 3 &&
            ImGui::MenuItem(trLabel("Remove reroute").c_str(), nullptr, false, !o.readOnly))
            result.removeReroutes.push_back(s.contextId);
        if (s.contextKind == 4 && ImGui::MenuItem(trLabel("Remove").c_str(), nullptr, false, !o.readOnly))
            result.removeNodes.push_back(s.contextId);
        if (s.contextKind == 2) {
            if (detail::inputText("##comment", s.groupLabel, ImGuiInputTextFlags_EnterReturnsTrue) &&
                !o.readOnly) {
                std::vector<NodeGroup> changed(o.groups.data, o.groups.data + o.groups.count);
                for (auto &g : changed)
                    if (g.id == s.contextId)
                        g.label = s.groupLabel;
                result.groups = {EditPhase::commit, std::move(changed)};
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(trLabel("Remove group").c_str(), nullptr, false, !o.readOnly))
                result.removeGroups.push_back(s.contextId);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    const bool overlayHit = !hitNode && !hitPin && (hitRoute || hitGroup);
    if (f.hovered && ImGui::IsMouseClicked(0) && overlayHit && !s.canvas.panning && !s.overlayMode) {
        s.focused = true;
        pick(hitRoute ? s.selectedReroutes : s.selectedGroups, hitRoute ? hitRoute->id : hitGroup->id,
             io.KeyCtrl || io.KeyShift);
        if (!o.readOnly) {
            s.grab = f.mouse;
            s.revision = o.revision;
            if (hitRoute) {
                s.overlayMode = 3;
                s.overlay = hitRoute->id;
                s.routeBefore.assign(o.reroutes.data, o.reroutes.data + o.reroutes.count);
                s.routePreview = s.routeBefore;
                result.reroutes = {EditPhase::begin, s.routePreview};
            } else {
                s.overlayMode = groupResize ? 2 : 1;
                s.overlay = hitGroup->id;
                s.groupBefore.assign(o.groups.data, o.groups.data + o.groups.count);
                s.groupPreview = s.groupBefore;
                result.groups = {EditPhase::begin, s.groupPreview};
                if (!groupResize && o.moveGroupContents) {
                    s.groupNodesBefore.clear();
                    auto inside = [&](Point at) {
                        return at.x >= hitGroup->position.x && at.y >= hitGroup->position.y &&
                               at.x <= hitGroup->position.x + hitGroup->size.x &&
                               at.y <= hitGroup->position.y + hitGroup->size.y;
                    };
                    for (const auto &node : nodes)
                        if (inside(node.position))
                            s.groupNodesBefore.push_back({node.id, node.position});
                    s.groupNodesPreview = s.groupNodesBefore;
                    s.groupRoutesBefore.clear();
                    for (std::size_t route = 0; route < o.reroutes.count; ++route)
                        s.groupRoutesBefore.push_back(o.reroutes[route]);
                    s.groupRoutesPreview = s.groupRoutesBefore;
                    result.moved = {EditPhase::begin, s.groupNodesPreview};
                    result.reroutes = {EditPhase::begin, s.groupRoutesPreview};
                }
            }
        }
    }
    if (s.overlayMode) {
        const Point delta{f.mouse.x - s.grab.x, f.mouse.y - s.grab.y};
        if (s.overlayMode == 1 && o.moveGroupContents) {
            s.groupNodesPreview = s.groupNodesBefore;
            for (auto &node : s.groupNodesPreview) {
                node.position.x += delta.x;
                node.position.y += delta.y;
            }
            s.groupRoutesPreview = s.groupRoutesBefore;
            const auto original = std::find_if(s.groupBefore.begin(), s.groupBefore.end(),
                                               [&](const auto &g) { return g.id == s.overlay; });
            if (original != s.groupBefore.end())
                for (auto &route : s.groupRoutesPreview)
                    if (route.position.x >= original->position.x &&
                        route.position.y >= original->position.y &&
                        route.position.x <= original->position.x + original->size.x &&
                        route.position.y <= original->position.y + original->size.y) {
                        route.position.x += delta.x;
                        route.position.y += delta.y;
                    }
            if (result.moved.phase != EditPhase::begin) {
                const auto phase = ImGui::IsMouseDown(0) ? EditPhase::update : EditPhase::commit;
                result.moved = {phase, s.groupNodesPreview};
                result.reroutes = {phase, s.groupRoutesPreview};
            }
        }
        if (s.overlayMode == 3) {
            s.routePreview = s.routeBefore;
            for (auto &r : s.routePreview)
                if (r.id == s.overlay) {
                    r.position.x += delta.x;
                    r.position.y += delta.y;
                }
            if (result.reroutes.phase != EditPhase::begin)
                result.reroutes = {ImGui::IsMouseDown(0) ? EditPhase::update : EditPhase::commit,
                                   s.routePreview};
        } else {
            s.groupPreview = s.groupBefore;
            for (auto &g : s.groupPreview)
                if (g.id == s.overlay) {
                    if (s.overlayMode == 1) {
                        g.position.x += delta.x;
                        g.position.y += delta.y;
                    } else {
                        g.size.x = std::max(80., g.size.x + delta.x);
                        g.size.y = std::max(50., g.size.y + delta.y);
                    }
                }
            if (result.groups.phase != EditPhase::begin)
                result.groups = {ImGui::IsMouseDown(0) ? EditPhase::update : EditPhase::commit,
                                 s.groupPreview};
        }
        if (!ImGui::IsMouseDown(0))
            s.overlayMode = 0;
    }
    if (f.hovered && ImGui::IsMouseClicked(0) && !s.canvas.panning && !overlayHit && !s.overlayMode) {
        s.focused = true;
        if (hitPin && !o.readOnly) {
            s.wire = hitPin->id;
            s.wiring = true;
            s.wireOutput = hitPin->output;
            s.revision = o.revision;
        } else if (hitNode) {
            const auto key = hitNode->id;
            pick(s.selection, key, io.KeyCtrl || io.KeyShift);
            if (!o.readOnly && selected(s.selection, key)) {
                const std::unordered_set<ItemId> picked(s.selection.begin(), s.selection.end());
                s.before.clear();
                for (const auto &n : nodes)
                    if (picked.count(n.id))
                        s.before.push_back({n.id, n.position});
                s.preview = s.before;
                s.grab = f.mouse;
                s.dragging = true;
                s.revision = o.revision;
                result.moved = {EditPhase::begin, s.preview};
            }
        } else if (hitLink) {
            pick(s.selectedLinks, hitLink->id, io.KeyCtrl || io.KeyShift);
        } else {
            if (!io.KeyCtrl && !io.KeyShift)
                clearSelection();
            s.boxStart = f.mouse;
            s.boxing = true;
        }
    } else if (ImGui::IsMouseClicked(0) && !f.hovered && !contextHovered)
        s.focused = false;
    if (s.dragging) {
        if (ImGui::IsMouseDown(0)) {
            s.preview = s.before;
            for (auto &p : s.preview) {
                p.position.x += f.mouse.x - s.grab.x;
                p.position.y += f.mouse.y - s.grab.y;
                if (!io.KeyAlt) {
                    p.position.x = snapValue(p.position.x, o.snap);
                    p.position.y = snapValue(p.position.y, o.snap);
                }
            }
            if (result.moved.phase != EditPhase::begin && (io.MouseDelta.x || io.MouseDelta.y))
                result.moved = {EditPhase::update, s.preview};
        } else {
            result.moved = {EditPhase::commit, s.preview};
            clear();
        }
    }
    if (s.wiring) {
        const auto *p = pinAt(s.wire);
        if (p)
            wire(s.wireOutput ? p->p : io.MousePos, s.wireOutput ? io.MousePos : p->p, palette().accent);
        if (!ImGui::IsMouseDown(0)) {
            if (hitPin && p && hitPin->id != p->id && hitPin->node != p->node &&
                hitPin->output != s.wireOutput) {
                const auto from = s.wireOutput ? s.wire : hitPin->id, to = s.wireOutput ? hitPin->id : s.wire;
                if (!o.canConnect || o.canConnect(from, to)) {
                    result.connectFrom = from;
                    result.connectTo = to;
                    result.connect = true;
                }
            }
            s.wire = 0;
            s.wiring = false;
        }
    }
    if (s.boxing) {
        const auto a = s.canvas.view.screen(s.boxStart, f.bounds.min);
        Rect box{{std::min(a.x, io.MousePos.x), std::min(a.y, io.MousePos.y)},
                 {std::max(a.x, io.MousePos.x), std::max(a.y, io.MousePos.y)}};
        d->AddRectFilled(box.min, box.max, palette().selection);
        d->AddRect(box.min, box.max, palette().accent, 0, 0, dp(1));
        if (!ImGui::IsMouseDown(0)) {
            for (const auto &c : cards)
                if (box.contains(c.rect.min) && box.contains(c.rect.max) && !chosen.count(c.n->id))
                    s.selection.push_back(c.n->id);
            for (std::size_t i = 0; i < o.groups.count; ++i) {
                const auto &g = o.groups[i];
                if (box.contains(s.canvas.view.screen(g.position, f.bounds.min)) &&
                    box.contains(s.canvas.view.screen({g.position.x + g.size.x, g.position.y + g.size.y},
                                                      f.bounds.min)) &&
                    !selected(s.selectedGroups, g.id))
                    s.selectedGroups.push_back(g.id);
            }
            for (std::size_t i = 0; i < o.reroutes.count; ++i)
                if (box.contains(s.canvas.view.screen(o.reroutes[i].position, f.bounds.min)) &&
                    !selected(s.selectedReroutes, o.reroutes[i].id))
                    s.selectedReroutes.push_back(o.reroutes[i].id);
            s.boxing = false;
        }
    }
    if (s.focused && f.focused && !io.WantTextInput) {
        ImGui::SetNextFrameWantCaptureKeyboard(true); // Hosts (Unreal) route the shortcuts here.
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            clearSelection();
            for (const auto &n : nodes)
                s.selection.push_back(n.id);
            for (const auto &link : links)
                s.selectedLinks.push_back(link.id);
            for (std::size_t i = 0; i < o.groups.count; ++i)
                s.selectedGroups.push_back(o.groups[i].id);
            for (std::size_t i = 0; i < o.reroutes.count; ++i)
                s.selectedReroutes.push_back(o.reroutes[i].id);
        }
        if (!o.readOnly && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            result.removeNodes = s.selection;
            result.removeGroups = s.selectedGroups;
            result.removeLinks = s.selectedLinks;
            result.removeReroutes = s.selectedReroutes;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && hasSelection())
            clipboardAction = 1;
        if (!o.readOnly && o.allocateId && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
            clipboardAction = 2;
        if (!o.readOnly && o.allocateId && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && hasSelection())
            clipboardAction = 3;
        if (ImGui::IsKeyPressed(ImGuiKey_Home) && !cards.empty()) {
            Point lo = nodes.front().position, hi = lo;
            for (const auto &n : nodes) {
                lo.x = std::min(lo.x, n.position.x);
                lo.y = std::min(lo.y, n.position.y);
                hi.x = std::max(hi.x, n.position.x + dp(o.width));
                hi.y = std::max(hi.y, n.position.y + dp(o.header + o.row * 8));
            }
            s.canvas.view.fit(lo, hi, f.bounds.size(), dp(24));
        }
    }
    if (clipboardAction) {
        try {
            auto source = clipboardAction == 2
                              ? NodeFragment{}
                              : copyGraphSelection(selection(), nodes, links, o.groups, o.reroutes);
            bool valid = true;
            if (clipboardAction == 2) {
                const auto *text = ImGui::GetClipboardText();
                valid = nodesFromJson(text ? text : "", source, s.clipboardError);
            }
            if (valid && (!source.nodes.empty() || !source.groups.empty())) {
                if (clipboardAction == 1) {
                    ImGui::SetClipboardText(nodesToJson(source).c_str());
                    s.clipboardError.clear();
                } else {
                    Point offset{30, 30};
                    if (clipboardAction == 2) {
                        Point lo{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
                        auto include = [&](Point at) {
                            lo.x = std::min(lo.x, at.x);
                            lo.y = std::min(lo.y, at.y);
                        };
                        for (const auto &n : source.nodes)
                            include(n.position);
                        for (const auto &g : source.groups)
                            include(g.position);
                        for (const auto &r : source.reroutes)
                            include(r.position);
                        const auto at = pasteAtContext ? s.contextPosition : f.mouse;
                        offset = {at.x - lo.x, at.y - lo.y};
                    }
                    if (cloneNodes(source, allocate, offset, result.created, s.clipboardError)) {
                        clearSelection();
                        for (const auto &n : result.created.nodes)
                            s.selection.push_back(n.id);
                        for (const auto &g : result.created.groups)
                            s.selectedGroups.push_back(g.id);
                        for (const auto &l : result.created.links)
                            s.selectedLinks.push_back(l.id);
                        for (const auto &r : result.created.reroutes)
                            s.selectedReroutes.push_back(r.id);
                    }
                }
            }
        } catch (const std::exception &e) {
            s.clipboardError = e.what();
        }
    }
    if (!s.clipboardError.empty())
        clippedText(d, {f.bounds.min.x + dp(8), f.bounds.max.y - dp(24)}, f.bounds.size().x - dp(16),
                    palette().error, tr(s.clipboardError.c_str()).c_str());
    return result;
}
} // namespace cyui
