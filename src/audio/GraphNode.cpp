#include "GraphNode.h"

#include <utility>

namespace broadcastmix::audio {

GraphNode::GraphNode(std::string id, GraphNodeType type)
    : id_(std::move(id))
    , type_(type) {}

const std::string& GraphNode::id() const noexcept {
    return id_;
}

GraphNodeType GraphNode::type() const noexcept {
    return type_;
}

void GraphNode::setLabel(std::string_view label) {
    const bool shouldSyncPerson = (type_ == GraphNodeType::Position) && (person_.empty() || person_ == label_);
    label_ = label;
    if (shouldSyncPerson) {
        person_ = label_;
    }
}

const std::string& GraphNode::label() const noexcept {
    return label_;
}

void GraphNode::setPerson(std::string_view person) {
    person_ = person;
    if (type_ == GraphNodeType::Position) {
        label_ = person_;
    }
}

const std::string& GraphNode::person() const noexcept {
    return person_.empty() ? label_ : person_;
}

void GraphNode::setRole(std::string_view role) {
    role_ = role;
}

const std::string& GraphNode::role() const noexcept {
    return role_;
}

void GraphNode::setSource(std::string_view source) {
    source_ = source;
}

const std::string& GraphNode::source() const noexcept {
    return source_;
}

void GraphNode::setProfileImagePath(std::string_view path) {
    profileImagePath_ = path;
}

const std::string& GraphNode::profileImagePath() const noexcept {
    return profileImagePath_;
}

void GraphNode::setPresetName(std::string_view preset) {
    presetName_ = preset;
}

const std::string& GraphNode::presetName() const noexcept {
    return presetName_;
}

void GraphNode::addInputChannel() {
    ++inputChannels_;
}

void GraphNode::setInputChannelCount(std::uint32_t count) {
    inputChannels_ = count;
}

void GraphNode::addOutputChannel() {
    ++outputChannels_;
}

void GraphNode::setOutputChannelCount(std::uint32_t count) {
    outputChannels_ = count;
}

std::uint32_t GraphNode::inputChannelCount() const noexcept {
    return inputChannels_;
}

std::uint32_t GraphNode::outputChannelCount() const noexcept {
    return outputChannels_;
}

void GraphNode::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
}

bool GraphNode::enabled() const noexcept {
    return enabled_;
}

} // namespace broadcastmix::audio
