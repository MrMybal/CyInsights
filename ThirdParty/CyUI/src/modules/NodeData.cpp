#include "cyui/modules/Nodes.h"
#include "cyui/detail/Json.h"
#include <unordered_map>
#include <unordered_set>
namespace cyui {
namespace {
using detail::Json;
Json idJson(ItemId id) {
    return Json::num(std::to_string(id));
}
Json pointJson(Point p) {
    auto j = Json::list();
    j.items = {Json::num(p.x), Json::num(p.y)};
    return j;
}
Point readPoint(const Json &j) {
    if (j.kind != Json::array || j.items.size() != 2)
        throw std::invalid_argument("Invalid point");
    Point p{double(j.items[0].num()), double(j.items[1].num())};
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        throw std::invalid_argument("Non-finite point");
    return p;
}
} // namespace
NodeFragment copyNodeSelection(ArrayView<ItemId> ids, ArrayView<Node> nodes, ArrayView<NodeLink> links,
                               ArrayView<NodeGroup> groups, ArrayView<NodeReroute> reroutes) {
    NodeSelection selection;
    for (std::size_t i = 0; i < ids.count; ++i) {
        selection.nodes.push_back(ids[i]);
        selection.groups.push_back(ids[i]); // legacy combined node/group selection
    }
    return copyGraphSelection(selection, nodes, links, groups, reroutes);
}
NodeFragment copyGraphSelection(const NodeSelection &selection, ArrayView<Node> nodes,
                                ArrayView<NodeLink> links, ArrayView<NodeGroup> groups,
                                ArrayView<NodeReroute> reroutes) {
    NodeFragment f;
    std::unordered_set<ItemId> chosen(selection.nodes.begin(), selection.nodes.end()), pins, connections;
    std::unordered_set<ItemId> pickedGroups(selection.groups.begin(), selection.groups.end());
    std::unordered_set<ItemId> pickedLinks(selection.links.begin(), selection.links.end());
    std::unordered_set<ItemId> pickedRoutes(selection.reroutes.begin(), selection.reroutes.end());
    std::unordered_map<ItemId, const NodeGroup *> groupIndex;
    for (std::size_t i = 0; i < groups.count; ++i)
        groupIndex[groups[i].id] = &groups[i];
    std::vector<ItemId> pending(selection.groups);
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto found = groupIndex.find(pending[i]);
        if (found != groupIndex.end()) {
            const auto &g = *found->second;
            for (std::size_t n = 0; n < nodes.count; ++n) {
                auto p = nodes[n].position;
                if (p.x >= g.position.x && p.y >= g.position.y && p.x <= g.position.x + g.size.x &&
                    p.y <= g.position.y + g.size.y)
                    chosen.insert(nodes[n].id);
            }
            for (std::size_t n = 0; n < groups.count; ++n) {
                const auto &child = groups[n];
                if (child.position.x >= g.position.x && child.position.y >= g.position.y &&
                    child.position.x + child.size.x <= g.position.x + g.size.x &&
                    child.position.y + child.size.y <= g.position.y + g.size.y &&
                    pickedGroups.insert(child.id).second)
                    pending.push_back(child.id);
            }
        }
    }
    for (std::size_t i = 0; i < groups.count; ++i)
        if (pickedGroups.count(groups[i].id))
            f.groups.push_back(groups[i]);
    for (std::size_t i = 0; i < reroutes.count; ++i)
        if (pickedRoutes.count(reroutes[i].id))
            pickedLinks.insert(reroutes[i].link);
    std::unordered_map<ItemId, ItemId> owners;
    for (std::size_t i = 0; i < nodes.count; ++i) {
        for (const auto &p : nodes[i].inputs)
            owners[p.id] = nodes[i].id;
        for (const auto &p : nodes[i].outputs)
            owners[p.id] = nodes[i].id;
    }
    for (std::size_t i = 0; i < links.count; ++i)
        if (pickedLinks.count(links[i].id)) {
            for (auto pin : {links[i].from, links[i].to})
                if (owners.count(pin))
                    chosen.insert(owners.at(pin));
        }
    for (std::size_t i = 0; i < nodes.count; ++i)
        if (chosen.count(nodes[i].id)) {
            f.nodes.push_back(nodes[i]);
            for (const auto &p : nodes[i].inputs)
                pins.insert(p.id);
            for (const auto &p : nodes[i].outputs)
                pins.insert(p.id);
        }
    for (std::size_t i = 0; i < links.count; ++i)
        if (pins.count(links[i].from) && pins.count(links[i].to)) {
            f.links.push_back(links[i]);
            connections.insert(links[i].id);
        }
    for (std::size_t i = 0; i < reroutes.count; ++i)
        if (connections.count(reroutes[i].link))
            f.reroutes.push_back(reroutes[i]);
    return f;
}
std::string nodesToJson(const NodeFragment &f) {
    auto j = Json::obj();
    j.fields["format"] = Json::str("cyui.nodes");
    j.fields["version"] = Json::num("1");
    for (auto name : {"nodes", "links", "groups", "reroutes"})
        j.fields[name] = Json::list();
    auto pins = [](const std::vector<NodePin> &values) {
        auto a = Json::list();
        for (const auto &p : values) {
            auto j = Json::obj();
            j.fields["id"] = idJson(p.id);
            j.fields["label"] = Json::str(p.label);
            j.fields["color"] = Json::num(std::to_string(p.colorIndex));
            a.items.push_back(j);
        }
        return a;
    };
    for (const auto &n : f.nodes) {
        auto v = Json::obj();
        v.fields["id"] = idJson(n.id);
        v.fields["position"] = pointJson(n.position);
        v.fields["label"] = Json::str(n.label);
        v.fields["color"] = Json::num(std::to_string(n.colorIndex));
        v.fields["inputs"] = pins(n.inputs);
        v.fields["outputs"] = pins(n.outputs);
        j.fields["nodes"].items.push_back(v);
    }
    for (const auto &l : f.links) {
        auto v = Json::obj();
        v.fields["id"] = idJson(l.id);
        v.fields["from"] = idJson(l.from);
        v.fields["to"] = idJson(l.to);
        v.fields["color"] = Json::num(std::to_string(l.colorIndex));
        j.fields["links"].items.push_back(v);
    }
    for (const auto &g : f.groups) {
        auto v = Json::obj();
        v.fields["id"] = idJson(g.id);
        v.fields["position"] = pointJson(g.position);
        v.fields["size"] = pointJson(g.size);
        v.fields["label"] = Json::str(g.label);
        v.fields["color"] = Json::num(std::to_string(g.colorIndex));
        j.fields["groups"].items.push_back(v);
    }
    for (const auto &r : f.reroutes) {
        auto v = Json::obj();
        v.fields["id"] = idJson(r.id);
        v.fields["link"] = idJson(r.link);
        v.fields["position"] = pointJson(r.position);
        j.fields["reroutes"].items.push_back(v);
    }
    auto text = detail::jsonDump(j);
    NodeFragment verified;
    std::string error;
    if (!nodesFromJson(text, verified, error))
        throw std::invalid_argument(error);
    return text;
}
bool nodesFromJson(const std::string &text, NodeFragment &out, std::string &error) {
    try {
        auto j = detail::jsonParse(text);
        if (j.get("format").str() != "cyui.nodes" || j.get("version").integer<int>() != 1)
            throw std::invalid_argument("Unsupported node format");
        NodeFragment f;
        std::unordered_set<ItemId> nodeIds, pinIds, linkIds, groupIds, routeIds, inputs, outputs, links;
        auto id = [](const Json &v, std::unordered_set<ItemId> &ids) {
            auto n = v.get("id").integer<ItemId>();
            if (!n || !ids.insert(n).second)
                throw std::invalid_argument("Duplicate or zero ID");
            return n;
        };
        auto list = [&](const char *key) -> const std::vector<Json> & {
            const auto &v = j.get(key);
            if (v.kind != Json::array)
                throw std::invalid_argument("Expected array");
            return v.items;
        };
        for (const auto &v : list("nodes")) {
            Node n;
            n.id = id(v, nodeIds);
            n.label = v.get("label").str();
            n.position = readPoint(v.get("position"));
            n.colorIndex = v.get("color").integer<int>();
            auto pins = [&](const char *key, std::vector<NodePin> &pins, std::unordered_set<ItemId> &set) {
                const auto &a = v.get(key);
                if (a.kind != Json::array)
                    throw std::invalid_argument("Expected pins array");
                for (const auto &p : a.items) {
                    auto pinId = id(p, pinIds);
                    pins.push_back({pinId, p.get("label").str(), p.get("color").integer<int>()});
                    set.insert(pinId);
                }
            };
            pins("inputs", n.inputs, inputs);
            pins("outputs", n.outputs, outputs);
            f.nodes.push_back(std::move(n));
        }
        for (const auto &v : list("links")) {
            NodeLink l{id(v, linkIds), v.get("from").integer<ItemId>(), v.get("to").integer<ItemId>(),
                       v.get("color").integer<int>()};
            if (!outputs.count(l.from) || !inputs.count(l.to))
                throw std::invalid_argument("Dangling or reversed link");
            links.insert(l.id);
            f.links.push_back(l);
        }
        for (const auto &v : list("groups")) {
            NodeGroup g{id(v, groupIds), readPoint(v.get("position")), readPoint(v.get("size")),
                        v.get("label").str(), v.get("color").integer<int>()};
            if (g.size.x <= 0 || g.size.y <= 0)
                throw std::invalid_argument("Invalid group size");
            f.groups.push_back(g);
        }
        for (const auto &v : list("reroutes")) {
            NodeReroute r{id(v, routeIds), v.get("link").integer<ItemId>(), readPoint(v.get("position"))};
            if (!links.count(r.link))
                throw std::invalid_argument("Dangling reroute");
            f.reroutes.push_back(r);
        }
        out = std::move(f);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool cloneNodes(const NodeFragment &source, const std::function<ItemId()> &allocate, Point offset,
                NodeFragment &out, std::string &error) {
    try {
        if (!allocate || !std::isfinite(offset.x) || !std::isfinite(offset.y))
            throw std::invalid_argument("Missing ID allocator or invalid offset");
        NodeFragment f;
        if (!nodesFromJson(nodesToJson(source), f, error))
            return false;
        std::unordered_map<ItemId, ItemId> pinMap, linkMap;
        std::unordered_set<ItemId> allocated;
        auto remap = [&](ItemId &key, std::unordered_map<ItemId, ItemId> *map = nullptr) {
            auto id = allocate();
            if (!id || !allocated.insert(id).second)
                throw std::invalid_argument("Allocator returned duplicate or zero ID");
            if (map)
                (*map)[key] = id;
            key = id;
        };
        auto move = [&](Point &p) {
            p.x += offset.x;
            p.y += offset.y;
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                throw std::invalid_argument("Position overflow");
        };
        for (auto &n : f.nodes) {
            remap(n.id);
            move(n.position);
            for (auto &p : n.inputs)
                remap(p.id, &pinMap);
            for (auto &p : n.outputs)
                remap(p.id, &pinMap);
        }
        for (auto &l : f.links) {
            auto from = pinMap.at(l.from), to = pinMap.at(l.to);
            remap(l.id, &linkMap);
            l.from = from;
            l.to = to;
        }
        for (auto &g : f.groups) {
            remap(g.id);
            move(g.position);
        }
        for (auto &r : f.reroutes) {
            auto link = linkMap.at(r.link);
            remap(r.id);
            r.link = link;
            move(r.position);
        }
        out = std::move(f);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace cyui
