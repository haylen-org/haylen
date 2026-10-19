#include "haylen/2d/animation/SkeletonData.hpp"

#include <algorithm>
#include <map>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace haylen::animation2d {

// Reads a document part by part, bones first, so every later reference finds what it names, and words every error with the part at fault.
class SkeletonData::Reader final {
  public:
    explicit Reader(SkeletonData& target) : data(target) {}

    void read(const core::Json& document) {
        requireObject(document, "A skeleton file");
        for (const auto& [key, value] : document.items()) {
            if (key != "atlas" && key != "bones" && key != "slots" && key != "skins" && key != "ik" && key != "transform" && key != "animations") {
                throw std::invalid_argument("A skeleton file has the unknown key \"" + key + "\".");
            }
        }
        readBones(document.value("bones", core::Json::array()));
        readSlots(document.value("slots", core::Json::array()));
        readSkins(document.value("skins", core::Json::object()));
        readIk(document.value("ik", core::Json::array()));
        readTransforms(document.value("transform", core::Json::array()));
        for (const auto& [name, value] : sorted(document.value("animations", core::Json::object()), "The animations of a skeleton")) {
            data.animations.push_back(readAnimation(name, value));
        }
    }

  private:
    static constexpr float kDegrees = std::numbers::pi_v<float> / 180.0F;

    static void requireObject(const core::Json& value, const std::string& what) {
        if (!value.is_object()) {
            throw std::invalid_argument(what + " must be an object.");
        }
    }

    [[nodiscard]] static std::map<std::string, core::Json> sorted(const core::Json& value, const std::string& what) {
        requireObject(value, what);
        return value.get<std::map<std::string, core::Json>>();
    }

    [[nodiscard]] static float number(const core::Json& object, const char* key, float fallback, const std::string& where) {
        const auto found = object.find(key);
        if (found == object.end()) {
            return fallback;
        }
        if (!found->is_number()) {
            throw std::invalid_argument("The key \"" + std::string(key) + "\" of " + where + " must be a number.");
        }
        return found->get<float>();
    }

    [[nodiscard]] static std::string text(const core::Json& object, const char* key, const std::string& where) {
        const auto found = object.find(key);
        if (found == object.end()) {
            return {};
        }
        if (!found->is_string()) {
            throw std::invalid_argument("The key \"" + std::string(key) + "\" of " + where + " must be a string.");
        }
        return found->get<std::string>();
    }

    [[nodiscard]] static math::Color color(const core::Json& object, const char* key, const std::string& where) {
        const std::string value = text(object, key, where);
        if (value.empty()) {
            return math::Color::white();
        }
        const std::optional<math::Color> parsed = math::Color::parse(value);
        if (!parsed) {
            throw std::invalid_argument("The key \"" + std::string(key) + "\" of " + where + " must be a color such as \"#FFFFFFFF\".");
        }
        return *parsed;
    }

    [[nodiscard]] static std::vector<float> numbers(const core::Json& object, const char* key, const std::string& where) {
        const auto found = object.find(key);
        if (found == object.end()) {
            return {};
        }
        if (!found->is_array() || !std::ranges::all_of(*found, [](const core::Json& value) { return value.is_number(); })) {
            throw std::invalid_argument("The key \"" + std::string(key) + "\" of " + where + " must be a list of numbers.");
        }
        return found->get<std::vector<float>>();
    }

    [[nodiscard]] std::size_t bone(const std::string& name, const std::string& where) const {
        const std::optional<std::size_t> found = data.findBone(name);
        if (!found) {
            throw std::invalid_argument(where + " names the unknown bone \"" + name + "\".");
        }
        return *found;
    }

    [[nodiscard]] std::size_t slot(const std::string& name, const std::string& where) const {
        const std::optional<std::size_t> found = data.findSlot(name);
        if (!found) {
            throw std::invalid_argument(where + " names the unknown slot \"" + name + "\".");
        }
        return *found;
    }

    void readBones(const core::Json& list) {
        for (const core::Json& entry : list) {
            requireObject(entry, "Every bone of a skeleton");
            const std::string name = text(entry, "name", "a bone");
            const std::string where = "the bone \"" + name + "\"";
            if (name.empty() || data.findBone(name)) {
                throw std::invalid_argument("Every bone of a skeleton needs a name of its own, and \"" + name + "\" is empty or taken.");
            }
            Bone parsed{
                .name = name,
                .position = {number(entry, "x", 0.0F, where), number(entry, "y", 0.0F, where)},
                .rotation = number(entry, "rotation", 0.0F, where) * kDegrees,
                .scale = {number(entry, "scaleX", 1.0F, where), number(entry, "scaleY", 1.0F, where)},
                .length = number(entry, "length", 0.0F, where),
            };
            const std::string parent = text(entry, "parent", where);
            if (!parent.empty()) {
                parsed.parent = bone(parent, "The bone \"" + name + "\"");
            } else if (!data.bones.empty()) {
                throw std::invalid_argument("The bone \"" + name + "\" needs a parent, since only the first bone is the root.");
            }
            data.bones.push_back(std::move(parsed));
        }
        if (data.bones.empty()) {
            throw std::invalid_argument("A skeleton needs at least one bone.");
        }
    }

    void readSlots(const core::Json& list) {
        for (const core::Json& entry : list) {
            requireObject(entry, "Every slot of a skeleton");
            const std::string name = text(entry, "name", "a slot");
            const std::string where = "the slot \"" + name + "\"";
            if (name.empty() || data.findSlot(name)) {
                throw std::invalid_argument("Every slot of a skeleton needs a name of its own, and \"" + name + "\" is empty or taken.");
            }
            Slot parsed{.name = name, .bone = bone(text(entry, "bone", where), "The slot \"" + name + "\""), .color = color(entry, "color", where), .attachment = text(entry, "attachment", where)};
            if (const std::string blend = text(entry, "blend", where); !blend.empty()) {
                const std::optional<graphics::BlendMode::Type> found = graphics::BlendMode::parse(blend);
                if (!found) {
                    throw std::invalid_argument("The slot \"" + name + "\" has the unknown blend mode \"" + blend + "\".");
                }
                if (graphics::BlendMode::readsBackdrop(*found)) {
                    throw std::invalid_argument("The slot \"" + name + "\" has the blend mode \"" + blend + "\", which mixes with what lies below and blends groups alone.");
                }
                parsed.blend = *found;
            }
            data.slots.push_back(std::move(parsed));
        }
    }

    void readSkins(const core::Json& described) {
        for (const auto& [skinName, slotsOfSkin] : sorted(described, "The skins of a skeleton")) {
            Skin skin{.name = skinName, .slots = std::vector<std::map<std::string, Attachment, std::less<>>>(data.slots.size())};
            for (const auto& [slotName, attachments] : sorted(slotsOfSkin, "The skin \"" + skinName + "\"")) {
                const std::size_t index = slot(slotName, "The skin \"" + skinName + "\"");
                for (const auto& [name, entry] : sorted(attachments, "The attachments of the slot \"" + slotName + "\"")) {
                    skin.slots[index].emplace(name, readAttachment(name, entry, "the attachment \"" + name + "\" of the slot \"" + slotName + "\""));
                }
            }
            data.skins.push_back(std::move(skin));
        }
        // The default skin always exists, so a slot without skins shows nothing rather than fail.
        if (!data.findSkin("default")) {
            data.skins.insert(data.skins.begin(), Skin{.name = "default", .slots = std::vector<std::map<std::string, Attachment, std::less<>>>(data.slots.size())});
        }
        for (const Slot& entry : data.slots) {
            const std::size_t index = static_cast<std::size_t>(&entry - data.slots.data());
            if (!entry.attachment.empty() && data.findAttachment(*data.findSkin("default"), index, entry.attachment) == nullptr && std::ranges::none_of(data.skins, [&](const Skin& skin) { return skin.slots[index].contains(entry.attachment); })) {
                throw std::invalid_argument("The slot \"" + entry.name + "\" starts with the attachment \"" + entry.attachment + "\", which no skin has.");
            }
        }
    }

    [[nodiscard]] Attachment readAttachment(const std::string& name, const core::Json& entry, const std::string& where) const {
        requireObject(entry, "The description of " + where);
        static const std::array<std::pair<std::string_view, Attachment::Kind>, 4> kKinds{{{"region", Attachment::Kind::Region}, {"mesh", Attachment::Kind::Mesh}, {"point", Attachment::Kind::Point}, {"boundingBox", Attachment::Kind::BoundingBox}}};
        const std::string type = text(entry, "type", where);
        const auto kind = std::ranges::find(kKinds, type.empty() ? std::string_view("region") : std::string_view(type), &std::pair<std::string_view, Attachment::Kind>::first);
        if (kind == kKinds.end()) {
            throw std::invalid_argument("The type of " + where + " must be \"region\", \"mesh\", \"point\" or \"boundingBox\".");
        }
        Attachment attachment{
            .kind = kind->second,
            .name = name,
            .frame = text(entry, "frame", where),
            .position = {number(entry, "x", 0.0F, where), number(entry, "y", 0.0F, where)},
            .rotation = number(entry, "rotation", 0.0F, where) * kDegrees,
            .scale = {number(entry, "scaleX", 1.0F, where), number(entry, "scaleY", 1.0F, where)},
            .size = {number(entry, "width", 0.0F, where), number(entry, "height", 0.0F, where)},
            .color = color(entry, "color", where),
        };
        if (attachment.kind == Attachment::Kind::Region || attachment.kind == Attachment::Kind::Mesh) {
            if (attachment.frame.empty()) {
                attachment.frame = name;
            }
            if (!data.atlas->hasFrame(attachment.frame)) {
                throw std::invalid_argument("The frame \"" + attachment.frame + "\" of " + where + " does not exist in the atlas of the skeleton.");
            }
        }
        if (attachment.kind == Attachment::Kind::BoundingBox) {
            attachment.vertices = pairs(numbers(entry, "vertices", where), where);
        }
        if (attachment.kind == Attachment::Kind::Mesh) {
            readMesh(attachment, entry, where);
        }
        return attachment;
    }

    [[nodiscard]] static std::vector<math::Vec2> pairs(const std::vector<float>& flat, const std::string& where) {
        if (flat.size() % 2 != 0) {
            throw std::invalid_argument("The vertices of " + where + " need an even count of numbers, an x and a y each.");
        }
        std::vector<math::Vec2> points;
        for (std::size_t index = 0; index < flat.size(); index += 2) {
            points.push_back({flat[index], flat[index + 1]});
        }
        return points;
    }

    // A mesh has its vertices in the space of its bone, or the weights of each vertex, the bones that move it with its place in the space of each.
    void readMesh(Attachment& attachment, const core::Json& entry, const std::string& where) const {
        attachment.uvs = pairs(numbers(entry, "uvs", where), where);
        for (const float index : numbers(entry, "triangles", where)) {
            attachment.triangles.push_back(static_cast<std::uint32_t>(index));
        }
        if (const auto weights = entry.find("weights"); weights != entry.end()) {
            attachment.influenceStarts.push_back(0);
            for (const core::Json& vertex : *weights) {
                for (const core::Json& influence : vertex) {
                    requireObject(influence, "Every weight of " + where);
                    attachment.influences.push_back({.bone = bone(text(influence, "bone", where), "A weight of " + where), .position = {number(influence, "x", 0.0F, where), number(influence, "y", 0.0F, where)}, .weight = number(influence, "weight", 1.0F, where)});
                }
                attachment.influenceStarts.push_back(attachment.influences.size());
            }
        } else {
            attachment.vertices = pairs(numbers(entry, "vertices", where), where);
        }
        const std::size_t count = attachment.getVertexCount();
        if (count < 3 || attachment.uvs.size() != count || attachment.triangles.empty() || attachment.triangles.size() % 3 != 0) {
            throw std::invalid_argument("The mesh " + where.substr(4) + " needs three vertices or more, one pair of texture coordinates for each and whole triangles.");
        }
        if (std::ranges::any_of(attachment.triangles, [count](std::uint32_t index) { return index >= count; })) {
            throw std::invalid_argument("A triangle of " + where + " refers to a vertex that does not exist.");
        }
    }

    void readIk(const core::Json& list) {
        for (const core::Json& entry : list) {
            requireObject(entry, "Every inverse kinematics constraint");
            const std::string name = text(entry, "name", "a constraint");
            const std::string where = "The inverse kinematics constraint \"" + name + "\"";
            IkConstraint constraint{.name = name, .target = bone(text(entry, "target", where), where), .bendPositive = entry.value("bendPositive", true), .mix = number(entry, "mix", 1.0F, where)};
            for (const core::Json& member : entry.value("bones", core::Json::array())) {
                constraint.bones.push_back(bone(member.is_string() ? member.get<std::string>() : std::string(), where));
            }
            if (constraint.bones.empty() || constraint.bones.size() > 2 || (constraint.bones.size() == 2 && data.bones[constraint.bones[1]].parent != constraint.bones[0])) {
                throw std::invalid_argument(where + " bends one bone, or a bone and its child.");
            }
            data.ikConstraints.push_back(std::move(constraint));
        }
    }

    void readTransforms(const core::Json& list) {
        for (const core::Json& entry : list) {
            requireObject(entry, "Every transform constraint");
            const std::string name = text(entry, "name", "a constraint");
            const std::string where = "The transform constraint \"" + name + "\"";
            TransformConstraint constraint{
                .name = name,
                .target = bone(text(entry, "target", where), where),
                .rotateMix = number(entry, "rotateMix", 1.0F, where),
                .translateMix = number(entry, "translateMix", 1.0F, where),
                .rotation = number(entry, "rotation", 0.0F, where) * kDegrees,
                .offset = {number(entry, "x", 0.0F, where), number(entry, "y", 0.0F, where)},
            };
            for (const core::Json& member : entry.value("bones", core::Json::array())) {
                constraint.bones.push_back(bone(member.is_string() ? member.get<std::string>() : std::string(), where));
            }
            data.transformConstraints.push_back(std::move(constraint));
        }
    }

    [[nodiscard]] static SkeletonAnimation::Curve curve(const core::Json& key, const std::string& where) {
        const auto found = key.find("curve");
        if (found == key.end()) {
            return {};
        }
        if (found->is_array() && found->size() == 4 && std::ranges::all_of(*found, [](const core::Json& value) { return value.is_number(); })) {
            return {.kind = SkeletonAnimation::Curve::Kind::Bezier, .bezier = {(*found)[0].get<float>(), (*found)[1].get<float>(), (*found)[2].get<float>(), (*found)[3].get<float>()}};
        }
        const std::string name = found->is_string() ? found->get<std::string>() : std::string();
        if (name == "linear") {
            return {};
        }
        if (name == "stepped") {
            return {.kind = SkeletonAnimation::Curve::Kind::Stepped};
        }
        const std::optional<math::Easing::Type> easing = math::Easing::parse(name);
        if (!easing) {
            throw std::invalid_argument("The curve of a key of " + where + " must be \"linear\", \"stepped\", the name of an easing or four numbers of a Bezier curve.");
        }
        return {.kind = SkeletonAnimation::Curve::Kind::Eased, .easing = *easing};
    }

    // Reads keys in order of time, each with the values its reader takes.
    template <typename Values> [[nodiscard]] static std::vector<SkeletonAnimation::Key> keys(const core::Json& list, const std::string& where, Values&& values) {
        if (!list.is_array() || list.empty()) {
            throw std::invalid_argument("The timeline of " + where + " needs a list of keys.");
        }
        std::vector<SkeletonAnimation::Key> parsed;
        for (const core::Json& entry : list) {
            requireObject(entry, "Every key of " + where);
            SkeletonAnimation::Key key{.time = number(entry, "time", 0.0F, where), .curve = curve(entry, where)};
            values(entry, key.values);
            if (!parsed.empty() && key.time < parsed.back().time) {
                throw std::invalid_argument("The keys of " + where + " must follow each other in time.");
            }
            parsed.push_back(key);
        }
        return parsed;
    }

    static void readPair(const core::Json& key, std::array<float, 4>& values, float fallback, const std::string& where) {
        values[0] = number(key, "x", fallback, where);
        values[1] = number(key, "y", fallback, where);
    }

    static void readColor(const core::Json& key, std::array<float, 4>& values, const std::string& where) {
        const math::Color value = color(key, "color", where);
        values = {value.r, value.g, value.b, value.a};
    }

    static void readIkKey(const core::Json& key, std::array<float, 4>& values, const std::string& where) {
        values[0] = number(key, "mix", 1.0F, where);
        values[1] = key.value("bendPositive", true) ? 1.0F : 0.0F;
    }

    [[nodiscard]] SkeletonAnimation readAnimation(const std::string& name, const core::Json& entry) {
        requireObject(entry, "The animation \"" + name + "\"");
        SkeletonAnimation animation{.name = name};
        const std::string in = " in the animation \"" + name + "\"";

        for (const auto& [boneName, properties] : sorted(entry.value("bones", core::Json::object()), "The bones" + in)) {
            const std::size_t target = bone(boneName, "The animation \"" + name + "\"");
            for (const auto& [property, list] : properties.items()) {
                const std::string where = "the " + property + " of the bone \"" + boneName + "\"" + in;
                if (property == "rotate") {
                    animation.timelines.push_back({.property = SkeletonAnimation::Property::Rotate, .target = target, .keys = keys(list, where, [&where](const core::Json& key, auto& values) { values[0] = number(key, "value", 0.0F, where) * kDegrees; })});
                } else if (property == "translate" || property == "scale") {
                    const float fallback = property == "scale" ? 1.0F : 0.0F;
                    animation.timelines.push_back({.property = property == "scale" ? SkeletonAnimation::Property::Scale : SkeletonAnimation::Property::Translate, .target = target, .keys = keys(list, where, [&where, fallback](const core::Json& key, auto& values) { readPair(key, values, fallback, where); })});
                } else {
                    throw std::invalid_argument("The bone \"" + boneName + "\"" + in + " has the unknown timeline \"" + property + "\". Bones take \"rotate\", \"translate\" and \"scale\".");
                }
            }
        }

        for (const auto& [slotName, properties] : sorted(entry.value("slots", core::Json::object()), "The slots" + in)) {
            const std::size_t target = slot(slotName, "The animation \"" + name + "\"");
            for (const auto& [property, list] : properties.items()) {
                const std::string where = "the " + property + " of the slot \"" + slotName + "\"" + in;
                if (property == "color") {
                    animation.timelines.push_back({.property = SkeletonAnimation::Property::Color, .target = target, .keys = keys(list, where, [&where](const core::Json& key, auto& values) { readColor(key, values, where); })});
                } else if (property == "attachment") {
                    SkeletonAnimation::Timeline timeline{.property = SkeletonAnimation::Property::Attachment, .target = target, .keys = keys(list, where, [](const core::Json&, auto&) {})};
                    for (const core::Json& key : list) {
                        timeline.names.push_back(text(key, "name", where));
                    }
                    animation.timelines.push_back(std::move(timeline));
                } else {
                    throw std::invalid_argument("The slot \"" + slotName + "\"" + in + " has the unknown timeline \"" + property + "\". Slots take \"attachment\" and \"color\".");
                }
            }
        }

        for (const auto& [slotName, attachments] : sorted(entry.value("deform", core::Json::object()), "The deforms" + in)) {
            const std::size_t target = slot(slotName, "The animation \"" + name + "\"");
            for (const auto& [attachmentName, list] : attachments.items()) {
                const std::string where = "the deform of \"" + attachmentName + "\" of the slot \"" + slotName + "\"" + in;
                SkeletonAnimation::Timeline timeline{.property = SkeletonAnimation::Property::Deform, .target = target, .keys = keys(list, where, [](const core::Json&, auto&) {}), .attachment = attachmentName};
                const Attachment* mesh = nullptr;
                for (const Skin& skin : data.skins) {
                    const auto found = skin.slots[target].find(attachmentName);
                    mesh = found != skin.slots[target].end() ? &found->second : mesh;
                }
                if (mesh == nullptr || mesh->kind != Attachment::Kind::Mesh) {
                    throw std::invalid_argument("The deform" + in + " names \"" + attachmentName + "\" of the slot \"" + slotName + "\", which is no mesh.");
                }
                const std::size_t expected = (mesh->isWeighted() ? mesh->influences.size() : mesh->vertices.size()) * 2;
                for (const core::Json& key : list) {
                    timeline.offsets.push_back(numbers(key, "offsets", where));
                    if (timeline.offsets.back().size() != expected) {
                        throw std::invalid_argument("Every key of " + where + " needs " + std::to_string(expected) + " offsets, two for each vertex or weight of the mesh.");
                    }
                }
                animation.timelines.push_back(std::move(timeline));
            }
        }

        for (const auto& [constraintName, list] : sorted(entry.value("ik", core::Json::object()), "The constraints" + in)) {
            const std::optional<std::size_t> target = data.findIkConstraint(constraintName);
            if (!target) {
                throw std::invalid_argument("The animation \"" + name + "\" names the unknown inverse kinematics constraint \"" + constraintName + "\".");
            }
            const std::string where = "the constraint \"" + constraintName + "\"" + in;
            animation.timelines.push_back({.property = SkeletonAnimation::Property::IkMix, .target = *target, .keys = keys(list, where, [&where](const core::Json& key, auto& values) { readIkKey(key, values, where); })});
        }

        for (const core::Json& key : entry.value("drawOrder", core::Json::array())) {
            SkeletonAnimation::DrawOrderKey order{.time = number(key, "time", 0.0F, "a draw order key" + in)};
            for (const core::Json& member : key.value("order", core::Json::array())) {
                order.order.push_back(slot(member.is_string() ? member.get<std::string>() : std::string(), "A draw order key" + in));
            }
            if (order.order.size() != data.slots.size()) {
                throw std::invalid_argument("Every draw order key" + in + " lists every slot once.");
            }
            animation.drawOrder.push_back(std::move(order));
        }

        for (const core::Json& key : entry.value("events", core::Json::array())) {
            const std::string where = "an event" + in;
            SkeletonAnimation::Event event{.time = number(key, "time", 0.0F, where), .name = text(key, "name", where), .text = text(key, "text", where), .number = number(key, "number", 0.0F, where)};
            if (event.name.empty()) {
                throw std::invalid_argument("Every event" + in + " needs a name.");
            }
            if (std::ranges::find(data.events, event.name) == data.events.end()) {
                data.events.push_back(event.name);
            }
            animation.events.push_back(std::move(event));
        }
        std::ranges::sort(animation.events, {}, &SkeletonAnimation::Event::time);

        // An animation lasts its duration, or until its last key.
        float last = 0.0F;
        for (const SkeletonAnimation::Timeline& timeline : animation.timelines) {
            last = std::max(last, timeline.keys.back().time);
        }
        for (const SkeletonAnimation::DrawOrderKey& order : animation.drawOrder) {
            last = std::max(last, order.time);
        }
        for (const SkeletonAnimation::Event& event : animation.events) {
            last = std::max(last, event.time);
        }
        animation.duration = number(entry, "duration", last, "the animation \"" + name + "\"");
        return animation;
    }

    SkeletonData& data;
};

SkeletonData SkeletonData::parse(const core::Json& document, std::shared_ptr<const SpriteAtlas> frames) {
    if (!frames) {
        throw std::invalid_argument("A skeleton needs the atlas its attachments show.");
    }
    SkeletonData data;
    data.atlas = std::move(frames);
    Reader(data).read(document);
    return data;
}

std::string SkeletonData::atlasPath(const core::Json& document) {
    if (!document.is_object() || !document.contains("atlas") || !document.at("atlas").is_string()) {
        throw std::invalid_argument("A skeleton file needs \"atlas\", the path of the atlas of its attachments.");
    }
    return document.at("atlas").get<std::string>();
}

std::optional<std::size_t> SkeletonData::findBone(std::string_view name) const noexcept {
    const auto found = std::ranges::find(bones, name, &Bone::name);
    return found != bones.end() ? std::optional(static_cast<std::size_t>(found - bones.begin())) : std::nullopt;
}

std::optional<std::size_t> SkeletonData::findSlot(std::string_view name) const noexcept {
    const auto found = std::ranges::find(slots, name, &Slot::name);
    return found != slots.end() ? std::optional(static_cast<std::size_t>(found - slots.begin())) : std::nullopt;
}

std::optional<std::size_t> SkeletonData::findSkin(std::string_view name) const noexcept {
    const auto found = std::ranges::find(skins, name, &Skin::name);
    return found != skins.end() ? std::optional(static_cast<std::size_t>(found - skins.begin())) : std::nullopt;
}

std::optional<std::size_t> SkeletonData::findIkConstraint(std::string_view name) const noexcept {
    const auto found = std::ranges::find(ikConstraints, name, &IkConstraint::name);
    return found != ikConstraints.end() ? std::optional(static_cast<std::size_t>(found - ikConstraints.begin())) : std::nullopt;
}

const SkeletonAnimation* SkeletonData::findAnimation(std::string_view name) const noexcept {
    const auto found = std::ranges::find(animations, name, &SkeletonAnimation::name);
    return found != animations.end() ? &*found : nullptr;
}

const SkeletonData::Attachment* SkeletonData::findAttachment(std::size_t skin, std::size_t slot, std::string_view name) const noexcept {
    for (const std::optional<std::size_t> source : {std::optional(skin), findSkin("default")}) {
        if (!source) {
            continue;
        }
        const auto& attachments = skins[*source].slots[slot];
        if (const auto found = attachments.find(name); found != attachments.end()) {
            return &found->second;
        }
    }
    return nullptr;
}

} // namespace haylen::animation2d
