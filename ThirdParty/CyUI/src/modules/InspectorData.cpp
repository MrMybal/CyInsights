#include "cyui/modules/Inspector.h"
#include "cyui/detail/Json.h"
#include <unordered_set>
#include <charconv>
#include <type_traits>
#include <climits>

namespace cyui {
namespace {
using detail::Json;
std::string key(const Property &p) {
    return p.key.empty() ? p.label : p.key;
}
template <class V> Json encode(const V &value, int depth = 0) {
    if (depth > 12)
        throw std::invalid_argument("Property nesting exceeds 12 levels");
    return std::visit(
        [depth](const auto &v) -> Json {
            using T = std::decay_t<decltype(v)>;
            Json j = Json::obj();
            if constexpr (std::is_same_v<T, bool>) {
                j.fields["type"] = Json::str("bool");
                j.fields["value"] = Json::flag(v);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                j.fields["type"] = Json::str("int64");
                j.fields["value"] = Json::num(std::to_string(v));
            } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
                j.fields["type"] = Json::str(std::is_same_v<T, float> ? "float" : "double");
                j.fields["value"] = Json::num(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                j.fields["type"] = Json::str("string");
                j.fields["value"] = Json::str(v);
            } else if constexpr (std::is_same_v<T, PropertyVector>) {
                j.fields["type"] = Json::str("vector" + std::to_string(v.values.size()));
                auto a = Json::list();
                for (auto n : v.values)
                    a.items.push_back(Json::num(n));
                j.fields["value"] = std::move(a);
            } else if constexpr (std::is_same_v<T, PropertyColor>) {
                j.fields["type"] = Json::str("color");
                auto a = Json::list();
                for (auto n : v.rgba)
                    a.items.push_back(Json::num(n));
                j.fields["value"] = std::move(a);
            } else if constexpr (std::is_same_v<T, PropertyAsset>) {
                j.fields["type"] = Json::str("asset");
                j.fields["assetType"] = Json::str(v.type);
                j.fields["value"] = Json::str(v.path);
            } else if constexpr (std::is_same_v<T, PropertyCustom>) {
                j.fields["type"] = Json::str("custom");
                j.fields["customType"] = Json::str(v.type);
                j.fields["value"] = detail::jsonParse(v.json);
            } else if constexpr (std::is_same_v<T, PropertyObject>) {
                j.fields["type"] = Json::str("object");
                j.fields["objectType"] = Json::str(v.type);
                auto fields = Json::obj();
                for (const auto &field : v.fields)
                    fields.fields[field.first] = encode(field.second, depth + 1);
                j.fields["value"] = std::move(fields);
            } else if constexpr (std::is_same_v<T, PropertyEnum>) {
                j.fields["type"] = Json::str(v.flags ? "flags" : "enum");
                j.fields["enumType"] = Json::str(v.type);
                j.fields["value"] = Json::num(std::to_string(v.value));
                auto items = Json::list();
                for (const auto &item : v.items) {
                    auto entry = Json::obj();
                    entry.fields["value"] = Json::num(std::to_string(item.first));
                    entry.fields["label"] = Json::str(item.second);
                    items.items.push_back(std::move(entry));
                }
                j.fields["items"] = std::move(items);
            } else if constexpr (std::is_same_v<T, PropertyOptional>) {
                j.fields["type"] = Json::str("optional");
                if (v.elementTemplate)
                    j.fields["element"] = encode(*v.elementTemplate, depth + 1);
                j.fields["value"] = v.value ? encode(*v.value, depth + 1) : Json{};
            } else if constexpr (std::is_same_v<T, PropertyMap>) {
                j.fields["type"] = Json::str("map");
                if (v.keyTemplate)
                    j.fields["keyTemplate"] = encode(*v.keyTemplate, depth + 1);
                if (v.valueTemplate)
                    j.fields["valueTemplate"] = encode(*v.valueTemplate, depth + 1);
                auto entries = Json::list();
                for (const auto &entry : v.entries) {
                    auto item = Json::obj();
                    item.fields["key"] = encode(entry.key, depth + 1);
                    item.fields["value"] = encode(entry.value, depth + 1);
                    entries.items.push_back(std::move(item));
                }
                j.fields["value"] = std::move(entries);
            } else {
                j.fields["type"] = Json::str(std::is_same_v<T, PropertySet> ? "set" : "array");
                if (v.elementTemplate)
                    j.fields["element"] = encode(*v.elementTemplate, depth + 1);
                auto a = Json::list();
                for (const auto &atom : v.values)
                    a.items.push_back(encode(atom, depth + 1));
                j.fields["value"] = std::move(a);
            }
            return j;
        },
        value);
}
PropertyValue decode(const Json &j, int depth = 0) {
    if (depth > 12)
        throw std::invalid_argument("Property nesting exceeds 12 levels");
    auto type = j.get("type").str();
    const auto &v = j.get("value");
    if (type == "bool")
        return v.flag();
    if (type == "int64")
        return v.integer<std::int64_t>();
    if (type == "double")
        return double(v.num());
    if (type == "float")
        return float(v.num());
    if (type == "string")
        return v.str();
    if (type == "asset")
        return PropertyAsset{j.get("assetType").str(), v.str()};
    if (type == "custom")
        return PropertyCustom{j.get("customType").str(), detail::jsonDump(v)};
    if (type == "enum" || type == "flags") {
        PropertyEnum e;
        e.type = j.get("enumType").str();
        e.value = v.integer<std::int64_t>();
        e.flags = type == "flags";
        if (j.fields.count("items")) {
            const auto &items = j.get("items");
            if (items.kind != Json::array)
                throw std::invalid_argument("Invalid enum items");
            for (const auto &item : items.items)
                e.items.emplace_back(item.get("value").integer<std::int64_t>(), item.get("label").str());
        }
        return e;
    }
    if (type == "optional") {
        PropertyOptional o;
        if (j.fields.count("element"))
            o.elementTemplate = std::make_shared<const PropertyValue>(decode(j.get("element"), depth + 1));
        if (v.kind != Json::null)
            o.value = std::make_unique<PropertyValue>(decode(v, depth + 1));
        return o;
    }
    if (type == "map") {
        if (v.kind != Json::array)
            throw std::invalid_argument("Invalid map");
        PropertyMap map;
        if (j.fields.count("keyTemplate"))
            map.keyTemplate = std::make_shared<const PropertyValue>(decode(j.get("keyTemplate"), depth + 1));
        if (j.fields.count("valueTemplate"))
            map.valueTemplate =
                std::make_shared<const PropertyValue>(decode(j.get("valueTemplate"), depth + 1));
        for (const auto &item : v.items)
            map.entries.push_back({decode(item.get("key"), depth + 1), decode(item.get("value"), depth + 1)});
        return map;
    }
    if (type == "object") {
        if (v.kind != Json::object)
            throw std::invalid_argument("Invalid object");
        PropertyObject object{j.get("objectType").str(), {}};
        for (const auto &field : v.fields)
            object.fields[field.first] = decode(field.second, depth + 1);
        return object;
    }
    if (type == "color") {
        if (v.kind != Json::array || v.items.size() != 4)
            throw std::invalid_argument("Invalid color");
        PropertyColor c;
        for (int i = 0; i < 4; ++i)
            c.rgba[i] = float(v.items[i].num());
        return c;
    }
    if (type == "vector2" || type == "vector3" || type == "vector4") {
        if (v.kind != Json::array || v.items.size() != std::size_t(type.back() - '0'))
            throw std::invalid_argument("Invalid vector");
        PropertyVector p;
        for (const auto &n : v.items)
            p.values.push_back(double(n.num()));
        return p;
    }
    if (type == "array" || type == "set") {
        if (v.kind != Json::array)
            throw std::invalid_argument("Invalid collection");
        PropertyArray a;
        if (j.fields.count("element"))
            a.elementTemplate = std::make_shared<PropertyValue>(decode(j.get("element"), depth + 1));
        for (const auto &item : v.items)
            a.values.push_back(decode(item, depth + 1));
        if (type == "set")
            return PropertySet{std::move(a.values), std::move(a.elementTemplate)};
        return a;
    }
    throw std::invalid_argument("Unknown property type");
}
Json envelope(const char *kind) {
    auto j = Json::obj();
    j.fields["format"] = Json::str("cyui.inspector");
    j.fields["version"] = Json::num("1");
    j.fields["kind"] = Json::str(kind);
    return j;
}
Json read(const std::string &text, const char *kind) {
    auto j = detail::jsonParse(text);
    if (j.get("format").str() != "cyui.inspector" || j.get("version").integer<int>() != 1 ||
        j.get("kind").str() != kind)
        throw std::invalid_argument("Unsupported clipboard format");
    return j;
}
PropertyEdit checked(const Property &p, PropertyValue v, const PropertyValidator &validate) {
    std::string error;
    if (p.readOnly)
        throw std::invalid_argument("Property is read-only");
    if (!normalizePropertyValue(p, v, error))
        throw std::invalid_argument(error);
    if (validate) {
        error = validate(p.id, v);
        if (!error.empty())
            throw std::invalid_argument(error);
    }
    return {p.id, {EditPhase::commit, std::move(v)}};
}
} // namespace
PropertyArray makePropertyArray(PropertyValue elementTemplate, std::size_t count) {
    if (count > std::size_t(INT_MAX))
        throw std::length_error("Too many collection rows");
    PropertyArray result;
    result.elementTemplate = std::make_shared<const PropertyValue>(std::move(elementTemplate));
    result.values.assign(count, *result.elementTemplate);
    return result;
}
PropertySet makePropertySet(PropertyValue prototype) {
    return {{}, std::make_shared<const PropertyValue>(std::move(prototype))};
}
PropertyMap makePropertyMap(PropertyValue key, PropertyValue value) {
    return {{},
            std::make_shared<const PropertyValue>(std::move(key)),
            std::make_shared<const PropertyValue>(std::move(value))};
}
PropertyOptional makePropertyOptional(PropertyValue prototype, bool present) {
    PropertyOptional result;
    result.elementTemplate = std::make_shared<const PropertyValue>(std::move(prototype));
    if (present)
        result.value = std::make_unique<PropertyValue>(*result.elementTemplate);
    return result;
}
static Property inheritedField(const Property &p, const PropertyValue &value) {
    Property field;
    field.id = p.id;
    field.value = value;
    field.readOnly = p.readOnly;
    field.bounded = p.bounded;
    field.minimum = p.minimum;
    field.maximum = p.maximum;
    field.integerMinimum = p.integerMinimum;
    field.integerMaximum = p.integerMaximum;
    field.choices = p.choices;
    field.password = p.password;
    field.multiline = p.multiline;
    return field;
}
Property propertyObjectField(const Property &root, const PropertyObject &object, const std::string &key) {
    auto field = inheritedField(root, object.fields.at(key));
    field.key = key;
    field.label = key;
    if (const auto found = object.metadata.find(key); found != object.metadata.end()) {
        const auto &m = found->second;
        field.label = m.label.empty() ? key : m.label;
        field.help = m.help;
        field.readOnly = root.readOnly || m.readOnly;
        field.password = m.password;
        field.multiline = m.multiline;
        field.bounded = m.bounded;
        field.minimum = m.minimum;
        field.maximum = m.maximum;
        field.integerMinimum = m.integerMinimum;
        field.integerMaximum = m.integerMaximum;
        field.choices = m.choices;
    }
    return field;
}
static Json identity(Json j) {
    const auto type = j.get("type").str();
    j.fields.erase("element");
    j.fields.erase("keyTemplate");
    j.fields.erase("valueTemplate");
    j.fields.erase("items");
    auto &value = j.fields.at("value");
    if (type == "array" || type == "set") {
        for (auto &item : value.items)
            item = identity(std::move(item));
        if (type == "set")
            std::sort(value.items.begin(), value.items.end(),
                      [](const auto &a, const auto &b) { return detail::jsonDump(a) < detail::jsonDump(b); });
    } else if (type == "map") {
        for (auto &item : value.items) {
            item.fields["key"] = identity(std::move(item.fields.at("key")));
            item.fields["value"] = identity(std::move(item.fields.at("value")));
        }
        std::sort(value.items.begin(), value.items.end(), [](const auto &a, const auto &b) {
            return detail::jsonDump(a.get("key")) < detail::jsonDump(b.get("key"));
        });
    } else if (type == "object")
        for (auto &field : value.fields)
            field.second = identity(std::move(field.second));
    else if (type == "optional" && value.kind != Json::null)
        value = identity(std::move(value));
    else if ((type == "float" || type == "double") && value.num() == 0)
        value = Json::num("0");
    else if (type == "color" || type == "vector2" || type == "vector3" || type == "vector4")
        for (auto &number : value.items)
            if (number.num() == 0)
                number = Json::num("0");
    return j;
}
std::string propertyValueKey(const PropertyValue &v) {
    return detail::jsonDump(identity(encode(v)));
}
bool PropertySet::operator==(const PropertySet &b) const {
    return propertyValueKey(*this) == propertyValueKey(b);
}
bool PropertyMap::operator==(const PropertyMap &b) const {
    return propertyValueKey(*this) == propertyValueKey(b);
}
std::string propertyValueText(const PropertyValue &v) {
    return std::visit(
        [&](const auto &n) -> std::string {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, std::string>)
                return n;
            else if constexpr (std::is_same_v<T, bool>)
                return n ? "true" : "false";
            else if constexpr (std::is_arithmetic_v<T>)
                return detail::jsonDump(encode(v).get("value"));
            else if constexpr (std::is_same_v<T, PropertyAsset>)
                return n.path;
            else if constexpr (std::is_same_v<T, PropertyCustom>)
                return n.type + ": " + n.json;
            else if constexpr (std::is_same_v<T, PropertyEnum>) {
                std::string text;
                for (const auto &item : n.items)
                    if (n.flags ? (item.first && (n.value & item.first) == item.first)
                                : n.value == item.first) {
                        if (!text.empty())
                            text += " | ";
                        text += item.second;
                    }
                return text.empty() ? std::to_string(n.value) : text;
            } else if constexpr (std::is_same_v<T, PropertyOptional>)
                return n.value ? propertyValueText(*n.value) : "null";
            else if constexpr (std::is_same_v<T, PropertyArray> || std::is_same_v<T, PropertySet>)
                return propertyType(v) + " (" + std::to_string(n.values.size()) + ")";
            else if constexpr (std::is_same_v<T, PropertyMap>)
                return "map (" + std::to_string(n.entries.size()) + ")";
            else if constexpr (std::is_same_v<T, PropertyObject>)
                return n.type;
            else
                return detail::jsonDump(encode(v).get("value"));
        },
        v);
}
std::string propertyType(const PropertyValue &v) {
    return std::visit(
        [](const auto &n) -> std::string {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, bool>)
                return "bool";
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return "int64";
            else if constexpr (std::is_same_v<T, double>)
                return "double";
            else if constexpr (std::is_same_v<T, float>)
                return "float";
            else if constexpr (std::is_same_v<T, std::string>)
                return "string";
            else if constexpr (std::is_same_v<T, PropertyVector>)
                return "vector" + std::to_string(n.values.size());
            else if constexpr (std::is_same_v<T, PropertyColor>)
                return "color";
            else if constexpr (std::is_same_v<T, PropertyAsset>)
                return "asset:" + n.type;
            else if constexpr (std::is_same_v<T, PropertyObject>)
                return "object:" + n.type;
            else if constexpr (std::is_same_v<T, PropertyCustom>)
                return "custom:" + n.type;
            else if constexpr (std::is_same_v<T, PropertySet>)
                return "set";
            else if constexpr (std::is_same_v<T, PropertyMap>)
                return "map";
            else if constexpr (std::is_same_v<T, PropertyOptional>)
                return "optional";
            else if constexpr (std::is_same_v<T, PropertyEnum>)
                return (n.flags ? "flags:" : "enum:") + n.type;
            else
                return "array";
        },
        v);
}
static std::string readonlySignature(const PropertyValue &schema, const PropertyValue &value, int depth = 0) {
    if (depth > 12 || propertyType(schema) != propertyType(value))
        return {};
    std::string signature;
    if (auto object = std::get_if<PropertyObject>(&schema)) {
        const auto &data = std::get<PropertyObject>(value);
        for (const auto &field : object->fields) {
            const auto actual = data.fields.find(field.first);
            if (actual == data.fields.end())
                continue;
            const auto metadata = object->metadata.find(field.first);
            const auto part = metadata != object->metadata.end() && metadata->second.readOnly
                                  ? propertyValueKey(actual->second)
                                  : readonlySignature(field.second, actual->second, depth + 1);
            if (!part.empty())
                signature += detail::jsonQuote(field.first) + ":" + part + ";";
        }
    } else if (auto array = std::get_if<PropertyArray>(&schema)) {
        const auto &data = std::get<PropertyArray>(value);
        const auto *prototype = array->elementTemplate
                                    ? array->elementTemplate.get()
                                    : (array->values.empty() ? nullptr : &array->values.front());
        if (prototype)
            for (std::size_t n = 0; n < data.values.size(); ++n) {
                auto part = readonlySignature(*prototype, data.values[n], depth + 1);
                if (!part.empty())
                    signature += std::to_string(n) + ":" + part + ";";
            }
    } else if (auto optional = std::get_if<PropertyOptional>(&schema)) {
        const auto &data = std::get<PropertyOptional>(value);
        const auto *prototype =
            optional->elementTemplate ? optional->elementTemplate.get() : optional->value.get();
        if (prototype && data.value)
            signature = readonlySignature(*prototype, *data.value, depth + 1);
    }
    return signature;
}
static bool normalizeAt(const Property &p, PropertyValue &v, std::string &error, int depth) {
    error.clear();
    try {
        if (depth > 12)
            throw std::invalid_argument("Property nesting exceeds 12 levels");
        if (propertyType(p.value) != propertyType(v))
            throw std::invalid_argument("Incompatible property type");
        auto number = [&](auto &n) {
            using T = std::decay_t<decltype(n)>;
            if (!std::isfinite(n))
                throw std::invalid_argument("Non-finite value");
            if (p.bounded) {
                if (!std::isfinite(p.minimum) || !std::isfinite(p.maximum) || p.minimum > p.maximum)
                    throw std::invalid_argument("Invalid numeric bounds");
                n = T(std::clamp(double(n), p.minimum, p.maximum));
                if (!std::isfinite(n))
                    throw std::invalid_argument("Numeric bound exceeds target type");
            }
        };
        std::visit(
            [&](auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, std::int64_t>) {
                    if (p.bounded) {
                        if (p.integerMinimum > p.integerMaximum)
                            throw std::invalid_argument("Invalid integer bounds");
                        n = std::clamp(n, p.integerMinimum, p.integerMaximum);
                    }
                } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
                    number(n);
                else if constexpr (std::is_same_v<T, PropertyVector>) {
                    if (n.values.size() < 2 || n.values.size() > 4)
                        throw std::invalid_argument("Expected vector2, vector3 or vector4");
                    for (auto &c : n.values)
                        number(c);
                } else if constexpr (std::is_same_v<T, PropertyColor>) {
                    for (auto &c : n.rgba) {
                        if (!std::isfinite(c))
                            throw std::invalid_argument("Non-finite color");
                        c = std::clamp(c, 0.f, 1.f);
                    }
                } else if constexpr (std::is_same_v<T, std::string>) {
                    if (!p.choices.empty() &&
                        std::find(p.choices.begin(), p.choices.end(), n) == p.choices.end())
                        throw std::invalid_argument("Value is not an available choice");
                } else if constexpr (std::is_same_v<T, PropertyArray>) {
                    if (n.values.size() > std::size_t(INT_MAX))
                        throw std::invalid_argument("Too many collection rows");
                    const auto &source = std::get<PropertyArray>(p.value);
                    const PropertyValue *prototype =
                        source.elementTemplate ? source.elementTemplate.get()
                                               : (source.values.empty() ? nullptr : &source.values.front());
                    if (!prototype)
                        prototype = n.elementTemplate ? n.elementTemplate.get()
                                                      : (n.values.empty() ? nullptr : &n.values.front());
                    std::optional<PropertyValue> stablePrototype;
                    if (prototype) {
                        stablePrototype = *prototype;
                        prototype = &*stablePrototype;
                    }
                    std::unordered_map<std::string, const PropertyValue *> originals;
                    if (prototype && !readonlySignature(*prototype, *prototype).empty())
                        for (const auto &element : source.values)
                            originals.emplace(readonlySignature(*prototype, element), &element);
                    auto normalizeElement = [&](PropertyValue &element,
                                                const PropertyValue *current = nullptr) {
                        Property field;
                        field.value = current && propertyType(*current) == propertyType(*prototype)
                                          ? *current
                                          : *prototype;
                        field.bounded = p.bounded;
                        field.minimum = p.minimum;
                        field.maximum = p.maximum;
                        field.integerMinimum = p.integerMinimum;
                        field.integerMaximum = p.integerMaximum;
                        field.choices = p.choices;
                        std::string e;
                        if (!normalizeAt(field, element, e, depth + 1))
                            throw std::invalid_argument(e);
                    };
                    if (prototype && n.elementTemplate) {
                        auto element = *n.elementTemplate;
                        normalizeElement(element);
                        n.elementTemplate = std::make_shared<const PropertyValue>(std::move(element));
                    }
                    for (std::size_t index = 0; index < n.values.size(); ++index) {
                        auto &atom = n.values[index];
                        const PropertyValue *current =
                            index < source.values.size() ? &source.values[index] : nullptr;
                        if (!originals.empty()) {
                            const auto found = originals.find(readonlySignature(*prototype, atom));
                            if (found != originals.end())
                                current = found->second;
                        }
                        normalizeElement(atom, current);
                    }
                    if (source.elementTemplate)
                        n.elementTemplate = source.elementTemplate;
                } else if constexpr (std::is_same_v<T, PropertySet>) {
                    const auto &source = std::get<PropertySet>(p.value);
                    auto field = inheritedField(p, PropertyArray{source.values, source.elementTemplate});
                    PropertyValue elements = PropertyArray{n.values, n.elementTemplate};
                    std::string e;
                    if (!normalizeAt(field, elements, e, depth))
                        throw std::invalid_argument(e);
                    auto array = std::get<PropertyArray>(std::move(elements));
                    std::unordered_set<std::string> identities;
                    for (const auto &element : array.values)
                        if (!identities.insert(propertyValueKey(element)).second)
                            throw std::invalid_argument("Duplicate set element");
                    n.values = std::move(array.values);
                    n.elementTemplate = std::move(array.elementTemplate);
                } else if constexpr (std::is_same_v<T, PropertyMap>) {
                    const auto &source = std::get<PropertyMap>(p.value);
                    const auto *key = source.keyTemplate
                                          ? source.keyTemplate.get()
                                          : (source.entries.empty() ? nullptr : &source.entries.front().key);
                    const auto *value =
                        source.valueTemplate
                            ? source.valueTemplate.get()
                            : (source.entries.empty() ? nullptr : &source.entries.front().value);
                    if (!key)
                        key = n.keyTemplate ? n.keyTemplate.get()
                                            : (n.entries.empty() ? nullptr : &n.entries.front().key);
                    if (!value)
                        value = n.valueTemplate ? n.valueTemplate.get()
                                                : (n.entries.empty() ? nullptr : &n.entries.front().value);
                    auto normalizePart = [&](const PropertyValue *prototype, PropertyValue &part,
                                             const PropertyValue *current = nullptr) {
                        if (!prototype)
                            throw std::invalid_argument("Map templates are required");
                        auto field = inheritedField(
                            p, current && propertyType(*current) == propertyType(*prototype) ? *current
                                                                                             : *prototype);
                        std::string e;
                        if (!normalizeAt(field, part, e, depth + 1))
                            throw std::invalid_argument(e);
                    };
                    if (key && n.keyTemplate) {
                        auto part = *n.keyTemplate;
                        normalizePart(key, part);
                    }
                    if (value && n.valueTemplate) {
                        auto part = *n.valueTemplate;
                        normalizePart(value, part);
                    }
                    std::unordered_set<std::string> keys;
                    std::unordered_map<std::string, const PropertyMapEntry *> originals;
                    for (const auto &entry : source.entries)
                        originals.emplace(propertyValueKey(entry.key), &entry);
                    for (std::size_t index = 0; index < n.entries.size(); ++index) {
                        auto &entry = n.entries[index];
                        const auto found = originals.find(propertyValueKey(entry.key));
                        const auto *current = found != originals.end()        ? found->second
                                              : index < source.entries.size() ? &source.entries[index]
                                                                              : nullptr;
                        normalizePart(key, entry.key, current ? &current->key : nullptr);
                        normalizePart(value, entry.value, current ? &current->value : nullptr);
                        if (!keys.insert(propertyValueKey(entry.key)).second)
                            throw std::invalid_argument("Duplicate map key");
                    }
                    if (source.keyTemplate)
                        n.keyTemplate = source.keyTemplate;
                    if (source.valueTemplate)
                        n.valueTemplate = source.valueTemplate;
                } else if constexpr (std::is_same_v<T, PropertyOptional>) {
                    const auto &source = std::get<PropertyOptional>(p.value);
                    const auto *prototype =
                        source.elementTemplate ? source.elementTemplate.get() : source.value.get();
                    if (!prototype)
                        throw std::invalid_argument("Optional template is required");
                    auto field = inheritedField(p, *prototype);
                    std::string e;
                    if (n.elementTemplate) {
                        auto part = *n.elementTemplate;
                        if (!normalizeAt(field, part, e, depth + 1))
                            throw std::invalid_argument(e);
                    }
                    if (n.value) {
                        auto current = inheritedField(p, source.value && propertyType(*source.value) ==
                                                                             propertyType(*prototype)
                                                             ? *source.value
                                                             : *prototype);
                        if (!normalizeAt(current, *n.value, e, depth + 1))
                            throw std::invalid_argument(e);
                    }
                    n.elementTemplate = source.elementTemplate
                                            ? source.elementTemplate
                                            : std::make_shared<const PropertyValue>(*prototype);
                } else if constexpr (std::is_same_v<T, PropertyEnum>) {
                    const auto &source = std::get<PropertyEnum>(p.value);
                    if (!source.items.empty())
                        n.items = source.items;
                    if (n.type.empty() || n.items.empty())
                        throw std::invalid_argument("Enum items and type are required");
                    std::unordered_set<std::int64_t> seen;
                    std::int64_t mask = 0;
                    for (const auto &item : n.items) {
                        if (!seen.insert(item.first).second || (n.flags && item.first < 0))
                            throw std::invalid_argument("Invalid enum items");
                        mask |= item.first;
                    }
                    if (n.flags ? (n.value < 0 || (n.value & ~mask) != 0) : !seen.count(n.value))
                        throw std::invalid_argument("Invalid enum value");
                } else if constexpr (std::is_same_v<T, PropertyObject>) {
                    const auto &source = std::get<PropertyObject>(p.value);
                    if (n.type.empty() || source.fields.size() != n.fields.size())
                        throw std::invalid_argument("Object schema mismatch");
                    for (auto &field : n.fields) {
                        const auto target = source.fields.find(field.first);
                        if (target == source.fields.end())
                            throw std::invalid_argument("Object schema mismatch");
                        auto child = propertyObjectField(p, source, field.first);
                        if (child.readOnly) {
                            if (!(field.second == target->second))
                                throw std::invalid_argument("Property is read-only");
                            continue;
                        }
                        std::string e;
                        if (!normalizeAt(child, field.second, e, depth + 1))
                            throw std::invalid_argument(e);
                    }
                    n.metadata = source.metadata;
                } else if constexpr (std::is_same_v<T, PropertyCustom>) {
                    if (n.type.empty())
                        throw std::invalid_argument("Custom type identifier is required");
                    n.json = detail::jsonDump(detail::jsonParse(n.json));
                }
            },
            v);
        return true;
    } catch (const std::invalid_argument &e) {
        error = e.what();
        return false;
    }
}
bool normalizePropertyValue(const Property &p, PropertyValue &v, std::string &error) {
    return normalizeAt(p, v, error, 0);
}
PropertyPasteResult editPropertyCollection(const Property &p, const std::vector<std::size_t> &selected,
                                           PropertyCollectionAction action, const std::string &text,
                                           std::size_t before, const PropertyValidator &validate) {
    try {
        if (p.readOnly)
            throw std::invalid_argument("Property is read-only");
        auto result = p.value;
        std::vector<PropertyValue> *values = nullptr;
        std::vector<PropertyMapEntry> *entries = nullptr;
        if (auto a = std::get_if<PropertyArray>(&result))
            values = &a->values;
        else if (auto s = std::get_if<PropertySet>(&result))
            values = &s->values;
        else if (auto m = std::get_if<PropertyMap>(&result))
            entries = &m->entries;
        else
            throw std::invalid_argument("Expected a collection");
        const auto size = values ? values->size() : entries->size();
        auto selection = selected;
        std::sort(selection.begin(), selection.end());
        selection.erase(std::unique(selection.begin(), selection.end()), selection.end());
        if (selection.empty() || selection.back() >= size)
            throw std::invalid_argument("Invalid collection selection");
        if (action == PropertyCollectionAction::remove) {
            for (auto it = selection.rbegin(); it != selection.rend(); ++it) {
                if (values)
                    values->erase(values->begin() + std::ptrdiff_t(*it));
                else
                    entries->erase(entries->begin() + std::ptrdiff_t(*it));
            }
        } else if (action == PropertyCollectionAction::duplicate ||
                   action == PropertyCollectionAction::move) {
            if (!std::holds_alternative<PropertyArray>(result))
                throw std::invalid_argument("Only arrays can be duplicated or reordered");
            std::vector<PropertyValue> copied;
            for (auto row : selection)
                copied.push_back((*values)[row]);
            if (before == std::size_t(-1))
                before = selection.back() + 1;
            if (before > size)
                throw std::invalid_argument("Invalid drop position");
            if (action == PropertyCollectionAction::move) {
                const auto removedBefore = std::size_t(
                    std::lower_bound(selection.begin(), selection.end(), before) - selection.begin());
                for (auto it = selection.rbegin(); it != selection.rend(); ++it)
                    values->erase(values->begin() + std::ptrdiff_t(*it));
                before -= removedBefore;
            }
            values->insert(values->begin() + std::ptrdiff_t(before), copied.begin(), copied.end());
        } else {
            for (auto row : selection) {
                auto &value = values ? (*values)[row] : (*entries)[row].value;
                const auto field = inheritedField(p, value);
                if (action == PropertyCollectionAction::expression) {
                    PropertyValue candidate;
                    std::string error;
                    if (!evaluatePropertyExpression(field, text, candidate, error))
                        throw std::invalid_argument(error);
                    value = std::move(candidate);
                } else {
                    auto paste = propertyFromJson(field, text);
                    if (!paste)
                        throw std::invalid_argument(paste.error);
                    value = std::move(paste.edits.front().edit.value);
                }
            }
        }
        return {{checked(p, std::move(result), validate)}, {}};
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
bool evaluatePropertyExpression(const Property &p, const std::string &expression, PropertyValue &out,
                                std::string &error) {
    out = p.value;
    error.clear();
    if (auto integer = std::get_if<std::int64_t>(&out)) {
        auto r = std::from_chars(expression.data(), expression.data() + expression.size(), *integer);
        if (r.ec != std::errc() || r.ptr != expression.data() + expression.size()) {
            auto original = std::get<std::int64_t>(p.value);
            if (std::numeric_limits<long double>::digits <= 53 &&
                (original > 9007199254740991LL || original < -9007199254740991LL)) {
                error = "Use an exact integer literal for values beyond 2^53";
                return false;
            }
            auto e = evaluateNumericExpression(expression, static_cast<long double>(original));
            if (!e.valid) {
                error = e.error;
                return false;
            }
            auto n = std::round(e.value);
            if (p.bounded)
                n = std::clamp(n, static_cast<long double>(p.integerMinimum),
                               static_cast<long double>(p.integerMaximum));
            if (n < -9223372036854775808.L || n >= 9223372036854775808.L) {
                error = "Integer overflow";
                return false;
            }
            *integer = static_cast<std::int64_t>(n);
        }
    } else if (std::holds_alternative<double>(out) || std::holds_alternative<float>(out)) {
        const auto original =
            std::holds_alternative<double>(out) ? std::get<double>(out) : std::get<float>(out);
        auto e = evaluateNumericExpression(expression, original);
        if (!e.valid) {
            error = e.error;
            return false;
        }
        if (p.bounded) {
            if (p.minimum > p.maximum) {
                error = "Invalid numeric bounds";
                return false;
            }
            e.value =
                std::clamp(e.value, static_cast<long double>(p.minimum), static_cast<long double>(p.maximum));
        }
        if (std::holds_alternative<double>(out))
            out = double(e.value);
        else
            out = float(e.value);
    } else {
        error = "Expected a numeric property";
        return false;
    }
    return normalizePropertyValue(p, out, error);
}
std::string propertyToJson(const Property &p) {
    auto j = envelope("property");
    j.fields["property"] = encode(p.value);
    auto text = detail::jsonDump(j);
    read(text, "property"); // exported clipboard data must fit the same budgets as import
    return text;
}
std::string propertyCategoryToJson(ArrayView<Property> fields, const std::string &category) {
    auto j = envelope("category");
    auto values = Json::obj();
    for (std::size_t i = 0; i < fields.count; ++i)
        if (fields[i].category == category) {
            if (!values.fields.emplace(key(fields[i]), encode(fields[i].value)).second)
                throw std::invalid_argument("Duplicate clipboard key");
        }
    j.fields["properties"] = std::move(values);
    auto text = detail::jsonDump(j);
    read(text, "category");
    return text;
}
PropertyPasteResult propertyFromJson(const Property &p, const std::string &text,
                                     const PropertyValidator &validate) {
    try {
        return {{checked(p, decode(read(text, "property").get("property")), validate)}, {}};
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
PropertyPasteResult propertyCategoryFromJson(ArrayView<Property> fields, const std::string &category,
                                             const std::string &text, const PropertyValidator &validate) {
    try {
        auto j = read(text, "category");
        const auto &values = j.get("properties");
        if (values.kind != Json::object)
            throw std::invalid_argument("Expected property map");
        PropertyPasteResult r;
        std::unordered_set<std::string> keys;
        for (std::size_t i = 0; i < fields.count; ++i)
            if (fields[i].category == category) {
                const auto &p = fields[i];
                if (!keys.insert(key(p)).second)
                    throw std::invalid_argument("Duplicate clipboard key");
                auto found = values.fields.find(key(p));
                if (found == values.fields.end())
                    throw std::invalid_argument("Category property is missing");
                r.edits.push_back(checked(p, decode(found->second), validate));
            }
        if (r.edits.empty() || r.edits.size() != values.fields.size())
            throw std::invalid_argument("Category schema does not match");
        return r;
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
static bool sameSchema(const PropertyValue &a, const PropertyValue &b, int depth = 0) {
    if (depth > 12 || propertyType(a) != propertyType(b))
        return false;
    if (auto array = std::get_if<PropertyArray>(&a)) {
        const auto &other = std::get<PropertyArray>(b);
        const auto *left = array->elementTemplate
                               ? array->elementTemplate.get()
                               : (array->values.empty() ? nullptr : &array->values.front());
        const auto *right = other.elementTemplate ? other.elementTemplate.get()
                                                  : (other.values.empty() ? nullptr : &other.values.front());
        return !left || !right || sameSchema(*left, *right, depth + 1);
    }
    if (auto set = std::get_if<PropertySet>(&a)) {
        const auto &other = std::get<PropertySet>(b);
        const auto *left = set->elementTemplate ? set->elementTemplate.get()
                                                : (set->values.empty() ? nullptr : &set->values.front());
        const auto *right = other.elementTemplate ? other.elementTemplate.get()
                                                  : (other.values.empty() ? nullptr : &other.values.front());
        return !left || !right || sameSchema(*left, *right, depth + 1);
    }
    if (auto map = std::get_if<PropertyMap>(&a)) {
        const auto &other = std::get<PropertyMap>(b);
        auto compatible = [&](const auto *left, const auto *right) {
            return !left || !right || sameSchema(*left, *right, depth + 1);
        };
        return compatible(map->keyTemplate       ? map->keyTemplate.get()
                          : map->entries.empty() ? nullptr
                                                 : &map->entries.front().key,
                          other.keyTemplate       ? other.keyTemplate.get()
                          : other.entries.empty() ? nullptr
                                                  : &other.entries.front().key) &&
               compatible(map->valueTemplate     ? map->valueTemplate.get()
                          : map->entries.empty() ? nullptr
                                                 : &map->entries.front().value,
                          other.valueTemplate     ? other.valueTemplate.get()
                          : other.entries.empty() ? nullptr
                                                  : &other.entries.front().value);
    }
    if (auto optional = std::get_if<PropertyOptional>(&a)) {
        const auto &other = std::get<PropertyOptional>(b);
        const auto *left =
            optional->elementTemplate ? optional->elementTemplate.get() : optional->value.get();
        const auto *right = other.elementTemplate ? other.elementTemplate.get() : other.value.get();
        return !left || !right || sameSchema(*left, *right, depth + 1);
    }
    if (auto object = std::get_if<PropertyObject>(&a)) {
        const auto &other = std::get<PropertyObject>(b);
        if (object->fields.size() != other.fields.size())
            return false;
        for (const auto &field : object->fields) {
            const auto target = other.fields.find(field.first);
            if (target == other.fields.end() || !sameSchema(field.second, target->second, depth + 1))
                return false;
        }
    }
    return true;
}
std::vector<Property> commonProperties(const std::vector<std::vector<Property>> &objects) {
    if (objects.empty())
        return {};
    std::vector<Property> result;
    for (auto p : objects.front()) {
        bool common = true;
        for (std::size_t i = 1; i < objects.size(); ++i) {
            auto found = std::find_if(objects[i].begin(), objects[i].end(), [&](const auto &v) {
                return v.category == p.category && key(v) == key(p);
            });
            if (found == objects[i].end() || !sameSchema(p.value, found->value)) {
                common = false;
                break;
            }
            p.mixed |= !(found->value == p.value);
            p.readOnly |= found->readOnly;
            if (found->bounded) {
                if (!p.bounded) {
                    p.minimum = found->minimum;
                    p.maximum = found->maximum;
                    p.integerMinimum = found->integerMinimum;
                    p.integerMaximum = found->integerMaximum;
                } else {
                    p.minimum = std::max(p.minimum, found->minimum);
                    p.maximum = std::min(p.maximum, found->maximum);
                    p.integerMinimum = std::max(p.integerMinimum, found->integerMinimum);
                    p.integerMaximum = std::min(p.integerMaximum, found->integerMaximum);
                }
                p.bounded = true;
            }
            if (!found->choices.empty()) {
                if (p.choices.empty())
                    p.choices = found->choices;
                else
                    p.choices.erase(std::remove_if(p.choices.begin(), p.choices.end(),
                                                   [&](const auto &choice) {
                                                       return std::find(found->choices.begin(),
                                                                        found->choices.end(),
                                                                        choice) == found->choices.end();
                                                   }),
                                    p.choices.end());
                if (p.choices.empty()) {
                    common = false;
                    break;
                }
            }
        }
        if (p.bounded && (std::holds_alternative<std::int64_t>(p.value) ? p.integerMinimum > p.integerMaximum
                                                                        : p.minimum > p.maximum))
            common = false;
        if (common)
            result.push_back(std::move(p));
    }
    return result;
}
} // namespace cyui
