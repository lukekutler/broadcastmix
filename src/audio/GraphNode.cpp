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
    label_ = label;
}

const std::string& GraphNode::label() const noexcept {
    return label_;
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
