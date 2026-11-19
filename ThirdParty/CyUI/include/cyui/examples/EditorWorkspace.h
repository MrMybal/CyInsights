#pragma once
// Example host model used by Gallery's integrated workspace and the integration tests.
// Widgets propose edits; this application layer owns validation, atomic commits and history.
#include "cyui/modules/Inspector.h"

namespace cyui::examples {
struct WorkspaceObject {
    ItemId id = 0;
    std::vector<Property> properties;
};
struct WorkspaceChange {
    ItemId object = 0, property = 0;
    PropertyValue value;
};
class EditorWorkspace {
    std::vector<WorkspaceObject> objects_;
    std::vector<std::vector<WorkspaceObject>> undo_, redo_;
    std::uint64_t revision_ = 0;

  public:
    std::string error;
    EditorWorkspace() {
        const char *names[]{"Camera", "Light", "Mesh"};
        for (ItemId id = 1; id <= 3; ++id) {
            WorkspaceObject object;
            object.id = id;
            auto add = [&](ItemId key, const char *label, PropertyValue value) {
                Property p;
                p.id = key;
                p.key = p.label = label;
                p.value = std::move(value);
                p.defaultValue = p.value;
                object.properties.push_back(std::move(p));
            };
            add(1, "Name", std::string(names[id - 1]));
            add(2, "Position", PropertyVector{{double(id * 75), double(id * 25), 0}});
            add(3, "Tint", PropertyColor{{.35f, .65f, .8f, 1}});
            add(4, "Texture", PropertyAsset{"Texture", "/Game/Textures/Terrain.png"});
            add(5, "Strength", 2.56);
            object.properties.back().bounded = true;
            object.properties.back().maximum = 10;
            PropertyMetadata tags;
            tags.choices = {"Environment", "Outdoor", "Static", "Dynamic"};
            PropertyMetadata overrides;
            overrides.bounded = true;
            overrides.minimum = 0;
            overrides.maximum = 10;
            add(6, "Settings",
                PropertyObject{
                    "Settings",
                    {{"Tags", PropertySet{{std::string("Environment"), std::string("Outdoor")},
                                          std::make_shared<const PropertyValue>(std::string("Static"))}},
                     {"Overrides", PropertyMap{{{std::string("Gain"), 1.}, {std::string("Threshold"), .5}},
                                               std::make_shared<const PropertyValue>(std::string{}),
                                               std::make_shared<const PropertyValue>(0.)}},
                     {"Quality", makePropertyOptional(
                                     PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "Medium"}, {2, "High"}}})}},
                    {{"Tags", tags}, {"Overrides", overrides}}});
            objects_.push_back(std::move(object));
        }
    }
    const std::vector<WorkspaceObject> &objects() const {
        return objects_;
    }
    const WorkspaceObject *find(ItemId id) const {
        const auto found =
            std::find_if(objects_.begin(), objects_.end(), [&](const auto &o) { return o.id == id; });
        return found == objects_.end() ? nullptr : &*found;
    }
    std::uint64_t revision() const {
        return revision_;
    }
    bool canUndo() const {
        return !undo_.empty();
    }
    bool canRedo() const {
        return !redo_.empty();
    }
    bool apply(const std::vector<WorkspaceChange> &changes, const PropertyValidator &validate = {}) {
        auto next = objects_;
        bool changed = false;
        error.clear();
        for (const auto &change : changes) {
            auto object =
                std::find_if(next.begin(), next.end(), [&](const auto &o) { return o.id == change.object; });
            if (object == next.end()) {
                error = "Unknown workspace object";
                return false;
            }
            auto property = std::find_if(object->properties.begin(), object->properties.end(),
                                         [&](const auto &p) { return p.id == change.property; });
            if (property == object->properties.end()) {
                error = "Unknown workspace property";
                return false;
            }
            if (property->readOnly) {
                error = "Property is read-only";
                return false;
            }
            auto value = change.value;
            if (!normalizePropertyValue(*property, value, error))
                return false;
            if (validate)
                error = validate(property->id, value);
            if (!error.empty())
                return false;
            changed |= !(property->value == value);
            property->value = std::move(value);
        }
        if (!changed)
            return false;
        undo_.push_back(objects_);
        if (undo_.size() > 100)
            undo_.erase(undo_.begin());
        objects_ = std::move(next);
        redo_.clear();
        ++revision_;
        return true;
    }
    bool undo() {
        if (!canUndo())
            return false;
        redo_.push_back(std::move(objects_));
        objects_ = std::move(undo_.back());
        undo_.pop_back();
        ++revision_;
        error.clear();
        return true;
    }
    bool redo() {
        if (!canRedo())
            return false;
        undo_.push_back(std::move(objects_));
        objects_ = std::move(redo_.back());
        redo_.pop_back();
        ++revision_;
        error.clear();
        return true;
    }
};
} // namespace cyui::examples
