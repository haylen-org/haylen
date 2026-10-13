#include "haylen/2d/animation/Skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::animation2d {

Skeleton::Skeleton(std::shared_ptr<const SkeletonData> skeletonData) : data(std::move(skeletonData)) {
    if (!data) {
        throw std::invalid_argument("A skeleton needs its data.");
    }
    skin = *data->findSkin("default");
    bones.resize(data->getBones().size());
    worlds.resize(bones.size());
    slots.resize(data->getSlots().size());
    ikMixes.resize(data->getIkConstraints().size());
    for (const SkeletonData::IkConstraint& constraint : data->getIkConstraints()) {
        ikSetupMixes.push_back(constraint.mix);
    }
    ikBends.resize(ikMixes.size());
    ikTargets.resize(ikMixes.size());
    setToSetupPose();
    computeWorld();
}

void Skeleton::setSkin(std::string_view name) {
    const std::optional<std::size_t> found = data->findSkin(name);
    if (!found) {
        throw std::invalid_argument("The skeleton has no skin \"" + std::string(name) + "\".");
    }
    skin = *found;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const std::string& setup = data->getSlots()[slot].attachment;
        slots[slot].attachment = setup.empty() ? nullptr : data->findAttachment(skin, slot, setup);
    }
}

const std::string& Skeleton::getSkin() const noexcept {
    return data->getSkins()[skin].name;
}

void Skeleton::setAttachment(std::string_view slot, std::string_view name) {
    const std::optional<std::size_t> found = data->findSlot(slot);
    if (!found) {
        throw std::invalid_argument("The skeleton has no slot \"" + std::string(slot) + "\".");
    }
    if (name.empty()) {
        slots[*found].attachment = nullptr;
        return;
    }
    const SkeletonData::Attachment* attachment = data->findAttachment(skin, *found, name);
    if (attachment == nullptr) {
        throw std::invalid_argument("The slot \"" + std::string(slot) + "\" has no attachment \"" + std::string(name) + "\" in the skin \"" + getSkin() + "\".");
    }
    slots[*found].attachment = attachment;
}

Skeleton::Track& Skeleton::getTrack(std::size_t index) {
    if (index >= tracks.size()) {
        tracks.resize(index + 1);
    }
    return tracks[index];
}

void Skeleton::play(std::string_view animation, const PlayOptions& options) {
    const SkeletonAnimation* found = data->findAnimation(animation);
    if (found == nullptr) {
        throw std::invalid_argument("The skeleton has no animation \"" + std::string(animation) + "\".");
    }
    if (!(options.fade >= 0.0F)) {
        throw std::invalid_argument("A fade lasts zero seconds or more.");
    }
    Track& track = getTrack(options.track);
    track.previous = options.fade > 0.0F ? track.current : std::nullopt;
    track.current = Entry{.animation = found, .loop = options.loop};
    track.fade = 0.0F;
    track.fadeDuration = options.fade;
}

void Skeleton::stop(std::size_t track) {
    if (track < tracks.size()) {
        tracks[track] = Track{.weight = tracks[track].weight};
    }
}

void Skeleton::setTrackWeight(std::size_t track, float value) {
    getTrack(track).weight = std::clamp(value, 0.0F, 1.0F);
}

float Skeleton::getTrackWeight(std::size_t track) const {
    return track < tracks.size() ? tracks[track].weight : 1.0F;
}

std::string_view Skeleton::getTrackAnimation(std::size_t track) const {
    return track < tracks.size() && tracks[track].current ? std::string_view(tracks[track].current->animation->name) : std::string_view();
}

float Skeleton::getTrackTime(std::size_t track) const {
    return track < tracks.size() && tracks[track].current ? tracks[track].current->time : 0.0F;
}

void Skeleton::seekTrack(std::size_t track, float time) {
    if (track < tracks.size() && tracks[track].current) {
        tracks[track].current->time = time;
        tracks[track].current->previousTime = time;
    }
}

std::size_t Skeleton::findIkConstraint(std::string_view name) const {
    const std::optional<std::size_t> found = data->findIkConstraint(name);
    if (!found) {
        throw std::invalid_argument("The skeleton has no inverse kinematics constraint \"" + std::string(name) + "\".");
    }
    return *found;
}

void Skeleton::setIkTarget(std::string_view constraint, math::Vec2 point) {
    ikTargets[findIkConstraint(constraint)] = point;
}

void Skeleton::clearIkTarget(std::string_view constraint) {
    ikTargets[findIkConstraint(constraint)].reset();
}

void Skeleton::setIkMix(std::string_view constraint, float value) {
    ikSetupMixes[findIkConstraint(constraint)] = std::clamp(value, 0.0F, 1.0F);
}

void Skeleton::setToSetupPose() {
    for (std::size_t bone = 0; bone < bones.size(); ++bone) {
        const SkeletonData::Bone& setup = data->getBones()[bone];
        bones[bone] = {.position = setup.position, .rotation = setup.rotation, .scale = setup.scale};
    }
    drawOrder.resize(slots.size());
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const SkeletonData::Slot& setup = data->getSlots()[slot];
        slots[slot].color = setup.color;
        slots[slot].attachment = setup.attachment.empty() ? nullptr : data->findAttachment(skin, slot, setup.attachment);
        slots[slot].deform.clear();
        drawOrder[slot] = slot;
    }
    for (std::size_t constraint = 0; constraint < ikMixes.size(); ++constraint) {
        ikMixes[constraint] = ikSetupMixes[constraint];
        ikBends[constraint] = data->getIkConstraints()[constraint].bendPositive;
    }
}

void Skeleton::save(Snapshot& snapshot) const {
    snapshot.bones = bones;
    snapshot.slots = slots;
    snapshot.drawOrder = drawOrder;
    snapshot.ikMixes = ikMixes;
}

void Skeleton::restore(const Snapshot& snapshot) {
    bones = snapshot.bones;
    slots = snapshot.slots;
    drawOrder = snapshot.drawOrder;
    ikMixes = snapshot.ikMixes;
}

void Skeleton::crossfade(const Track& track, float mix) {
    save(before);
    track.previous->animation->apply(*this, track.previous->time, track.previous->loop, 1.0F);
    save(faded);
    restore(before);
    track.current->animation->apply(*this, track.current->time, track.current->loop, 1.0F);

    // The pose before the track gives way to the blend of both animations by the weight, and switches such as attachments come from the animation that holds more than half.
    const float weight = track.weight;
    for (std::size_t bone = 0; bone < bones.size(); ++bone) {
        const BonePose& from = faded.bones[bone];
        BonePose& to = bones[bone];
        const BonePose& base = before.bones[bone];
        const BonePose blended{.position = math::Vec2::lerp(from.position, to.position, mix), .rotation = from.rotation + std::remainder(to.rotation - from.rotation, std::numbers::pi_v<float> * 2.0F) * mix, .scale = math::Vec2::lerp(from.scale, to.scale, mix)};
        to = {.position = math::Vec2::lerp(base.position, blended.position, weight), .rotation = base.rotation + std::remainder(blended.rotation - base.rotation, std::numbers::pi_v<float> * 2.0F) * weight, .scale = math::Vec2::lerp(base.scale, blended.scale, weight)};
    }
    const bool switched = mix * weight >= 0.5F;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        SlotPose& to = slots[slot];
        const SlotPose& from = faded.slots[slot];
        to.color = math::Color::lerp(before.slots[slot].color, math::Color::lerp(from.color, to.color, mix), weight);
        if (!switched) {
            to.attachment = weight * (1.0F - mix) >= 0.5F ? from.attachment : before.slots[slot].attachment;
        }
        if (to.deform.size() == from.deform.size()) {
            for (std::size_t place = 0; place < to.deform.size(); ++place) {
                to.deform[place] = from.deform[place] + (to.deform[place] - from.deform[place]) * mix;
            }
        }
    }
    if (!switched) {
        drawOrder = weight * (1.0F - mix) >= 0.5F ? faded.drawOrder : before.drawOrder;
    }
    for (std::size_t constraint = 0; constraint < ikMixes.size(); ++constraint) {
        ikMixes[constraint] = before.ikMixes[constraint] + (faded.ikMixes[constraint] + (ikMixes[constraint] - faded.ikMixes[constraint]) * mix - before.ikMixes[constraint]) * weight;
    }
}

// Bones come after their parents in the data, so one pass in order composes every world transform.
void Skeleton::computeWorld() {
    const math::Transform2D root = math::Transform2D::compose(position, rotation, scale);
    for (std::size_t bone = 0; bone < bones.size(); ++bone) {
        const BonePose& local = bones[bone];
        const math::Transform2D own = math::Transform2D::compose(local.position, local.rotation, local.scale);
        const std::optional<std::size_t> parent = data->getBones()[bone].parent;
        worlds[bone] = (parent ? worlds[*parent] : root) * own;
    }
}

float Skeleton::worldAngleOf(std::size_t bone) const noexcept {
    return std::atan2(worlds[bone].b, worlds[bone].a);
}

void Skeleton::aimBone(std::size_t bone, float worldAngle, float mix) {
    const std::optional<std::size_t> parent = data->getBones()[bone].parent;
    const math::Transform2D& parentWorld = parent ? worlds[*parent] : worlds[bone];
    const float parentAngle = parent ? std::atan2(parentWorld.b, parentWorld.a) : rotation;
    const float wanted = worldAngle - parentAngle;
    constexpr float kTurn = std::numbers::pi_v<float> * 2.0F;
    float delta = std::fmod(wanted - bones[bone].rotation, kTurn);
    if (delta > std::numbers::pi_v<float>) {
        delta -= kTurn;
    } else if (delta < -std::numbers::pi_v<float>) {
        delta += kTurn;
    }
    bones[bone].rotation += delta * mix;
}

// One bone points at the target, and two bones bend at the joint by the law of cosines so the end of the child reaches as near the target as their lengths allow.
void Skeleton::solveIk(std::size_t index) {
    const SkeletonData::IkConstraint& constraint = data->getIkConstraints()[index];
    const float mix = ikMixes[index];
    if (mix <= 0.0F) {
        return;
    }
    const math::Vec2 target = ikTargets[index].value_or(worlds[constraint.target].getTranslation());
    const std::size_t first = constraint.bones.front();
    const math::Vec2 origin = worlds[first].getTranslation();
    if (constraint.bones.size() == 1) {
        aimBone(first, (target - origin).getAngle(), mix);
        computeWorld();
        return;
    }

    const std::size_t second = constraint.bones[1];
    const math::Vec2 joint = worlds[second].getTranslation();
    const float upper = (joint - origin).getLength();
    const float lower = data->getBones()[second].length * math::Vec2{worlds[second].a, worlds[second].b}.getLength();
    const float reach = std::clamp((target - origin).getLength(), std::fabs(upper - lower), upper + lower);
    const float cosine = upper > 0.0F && reach > 0.0F ? std::clamp((upper * upper + reach * reach - lower * lower) / (2.0F * upper * reach), -1.0F, 1.0F) : 1.0F;
    const float bend = std::acos(cosine) * (ikBends[index] ? 1.0F : -1.0F);
    const float toTarget = (target - origin).getAngle();

    // The child points from where the parent puts the joint toward the target, measured from the joint the parent ends at.
    const float jointAngle = (joint - origin).isZero() ? worldAngleOf(first) : (joint - origin).getAngle();
    aimBone(first, worldAngleOf(first) + (toTarget - bend) - jointAngle, mix);
    computeWorld();
    aimBone(second, (target - worlds[second].getTranslation()).getAngle(), mix);
    computeWorld();
}

void Skeleton::solveTransform(const SkeletonData::TransformConstraint& constraint) {
    const math::Transform2D& target = worlds[constraint.target];
    for (const std::size_t bone : constraint.bones) {
        if (constraint.rotateMix > 0.0F) {
            aimBone(bone, worldAngleOf(constraint.target) + constraint.rotation, constraint.rotateMix);
        }
        if (constraint.translateMix > 0.0F) {
            const std::optional<std::size_t> parent = data->getBones()[bone].parent;
            const math::Transform2D parentWorld = parent ? worlds[*parent] : math::Transform2D::compose(position, rotation, scale);
            const math::Vec2 wanted = parentWorld.getInverse().apply(target.apply(constraint.offset));
            bones[bone].position += (wanted - bones[bone].position) * constraint.translateMix;
        }
        computeWorld();
    }
}

void Skeleton::pose() {
    setToSetupPose();
    for (const Track& track : tracks) {
        if (!track.current || track.weight <= 0.0F) {
            continue;
        }
        const float mix = track.previous && track.fadeDuration > 0.0F ? std::min(1.0F, track.fade / track.fadeDuration) : 1.0F;
        if (mix < 1.0F) {
            crossfade(track, mix);
        } else {
            track.current->animation->apply(*this, track.current->time, track.current->loop, track.weight);
        }
    }
    computeWorld();
    for (std::size_t constraint = 0; constraint < ikMixes.size(); ++constraint) {
        solveIk(constraint);
    }
    for (const SkeletonData::TransformConstraint& constraint : data->getTransformConstraints()) {
        solveTransform(constraint);
    }
}

void Skeleton::update(float deltaSeconds) {
    for (Track& track : tracks) {
        if (!track.current) {
            continue;
        }
        track.current->previousTime = track.current->time;
        track.current->time += deltaSeconds;
        if (track.previous) {
            track.previous->time += deltaSeconds;
            track.fade += deltaSeconds;
            if (track.fade >= track.fadeDuration) {
                track.previous.reset();
            }
        }
    }
    pose();

    // Listeners may play other animations, so the events of a track come from what it played when the update began.
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        if (!tracks[index].current) {
            continue;
        }
        const Entry entry = *tracks[index].current;
        passed.clear();
        entry.animation->collectEvents(entry.previousTime, entry.time, entry.loop, passed);
        for (const SkeletonAnimation::Event* event : passed) {
            eventFired.emit({.track = index, .animation = entry.animation->name, .name = event->name, .text = event->text, .number = event->number});
        }
    }
}

std::optional<math::Vec2> Skeleton::getPoint(std::string_view slot) const {
    const std::optional<std::size_t> found = data->findSlot(slot);
    if (!found) {
        throw std::invalid_argument("The skeleton has no slot \"" + std::string(slot) + "\".");
    }
    const SkeletonData::Attachment* attachment = slots[*found].attachment;
    if (attachment == nullptr || attachment->kind != SkeletonData::Attachment::Kind::Point) {
        return std::nullopt;
    }
    return worlds[data->getSlots()[*found].bone].apply(attachment->position);
}

// Regions are the quads of their frames in the space of their bone, trimmed frames cut down to the pixels the atlas keeps, and meshes place each vertex by its bone, or by the weights of its bones, plus the offsets of a deform.
void Skeleton::appendSlot(std::size_t slot) {
    const SlotPose& pose = slots[slot];
    const SkeletonData::Attachment& attachment = *pose.attachment;
    const SpriteFrame& frame = data->getAtlas().getFrame(attachment.frame);
    const math::Vec2 textureSize = data->getAtlas().getTexture().getSize();
    const math::Color tint = color * pose.color * attachment.color;
    const math::Transform2D& bone = worlds[data->getSlots()[slot].bone];
    const auto first = static_cast<std::uint32_t>(vertices.size());
    const auto uvOf = [&frame, textureSize](math::Vec2 share) { return (frame.source.getMin() + share * frame.source.getSize()) / textureSize; };

    if (attachment.kind == SkeletonData::Attachment::Kind::Region) {
        const math::Vec2 size = attachment.size.isZero() ? frame.originalSize : attachment.size;
        const math::Vec2 stretch = size / frame.originalSize;
        const math::Vec2 corner = -size * 0.5F + frame.offset * stretch;
        const math::Vec2 extent = frame.source.getSize() * stretch;
        const math::Transform2D place = bone * math::Transform2D::compose(attachment.position, attachment.rotation, attachment.scale);
        const std::array<math::Vec2, 4> shares{math::Vec2{0.0F, 0.0F}, math::Vec2{1.0F, 0.0F}, math::Vec2{1.0F, 1.0F}, math::Vec2{0.0F, 1.0F}};
        for (const math::Vec2 share : shares) {
            vertices.push_back({.position = place.apply(corner + extent * share), .uv = uvOf(share), .color = tint});
        }
        for (const std::uint32_t index : {0U, 1U, 2U, 0U, 2U, 3U}) {
            indices.push_back(first + index);
        }
        return;
    }

    const std::size_t count = attachment.getVertexCount();
    for (std::size_t vertex = 0; vertex < count; ++vertex) {
        math::Vec2 placed{};
        if (attachment.isWeighted()) {
            for (std::size_t influence = attachment.influenceStarts[vertex]; influence < attachment.influenceStarts[vertex + 1]; ++influence) {
                const SkeletonData::Attachment::Influence& entry = attachment.influences[influence];
                const math::Vec2 offset = pose.deform.size() > influence * 2 + 1 ? math::Vec2{pose.deform[influence * 2], pose.deform[influence * 2 + 1]} : math::Vec2{};
                placed += worlds[entry.bone].apply(entry.position + offset) * entry.weight;
            }
        } else {
            const math::Vec2 offset = pose.deform.size() > vertex * 2 + 1 ? math::Vec2{pose.deform[vertex * 2], pose.deform[vertex * 2 + 1]} : math::Vec2{};
            placed = bone.apply(attachment.vertices[vertex] + offset);
        }
        vertices.push_back({.position = placed, .uv = uvOf(attachment.uvs[vertex]), .color = tint});
    }
    for (const std::uint32_t index : attachment.triangles) {
        indices.push_back(first + index);
    }
}

math::Rect Skeleton::getBounds() {
    vertices.clear();
    indices.clear();
    for (const std::size_t slot : drawOrder) {
        const SkeletonData::Attachment* attachment = slots[slot].attachment;
        if (attachment != nullptr && (attachment->kind == SkeletonData::Attachment::Kind::Region || attachment->kind == SkeletonData::Attachment::Kind::Mesh)) {
            appendSlot(slot);
        }
    }
    if (vertices.empty()) {
        return math::Rect::fromCenter(position, {});
    }
    math::Rect bounds = math::Rect::fromCenter(vertices.front().position, {});
    for (const graphics2d::MeshVertex& vertex : vertices) {
        bounds = bounds.merged(math::Rect::fromCenter(vertex.position, {}));
    }
    return bounds;
}

// Slots draw in draw order, and neighbouring slots of one blend mode go into one mesh draw.
void Skeleton::draw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order) {
    vertices.clear();
    indices.clear();
    graphics2d::DrawOrder run = order;
    for (const std::size_t slot : drawOrder) {
        const SkeletonData::Attachment* attachment = slots[slot].attachment;
        if (attachment == nullptr || (attachment->kind != SkeletonData::Attachment::Kind::Region && attachment->kind != SkeletonData::Attachment::Kind::Mesh)) {
            continue;
        }
        const graphics::BlendMode::Type blend = data->getSlots()[slot].blend == graphics::BlendMode::Type::Alpha ? order.blend : data->getSlots()[slot].blend;
        if (!indices.empty() && blend != run.blend) {
            renderer.drawMesh(data->getAtlas().getTexture(), vertices, indices, run);
            vertices.clear();
            indices.clear();
        }
        run.blend = blend;
        appendSlot(slot);
    }
    if (!indices.empty()) {
        renderer.drawMesh(data->getAtlas().getTexture(), vertices, indices, run);
    }
}

} // namespace haylen::animation2d
