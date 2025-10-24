#include "NodeLibraryComponent.h"

namespace broadcastmix::app {

namespace {

juce::Colour toColour(const ui::Color& color) {
    return juce::Colour::fromFloatRGBA(color.r, color.g, color.b, color.a);
}

} // namespace

NodeLibraryComponent::NodeLibraryComponent() {
    heading_.setFont(juce::Font(juce::FontOptions { 18.0F, juce::Font::bold }));
    heading_.setJustificationType(juce::Justification::centredLeft);
    heading_.setInterceptsMouseClicks(false, false);

    hint_.setFont(juce::Font(juce::FontOptions { 13.0F }));
    hint_.setJustificationType(juce::Justification::centredLeft);
    hint_.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(heading_);
    addAndMakeVisible(hint_);
    addAndMakeVisible(viewport_);

    viewport_.setScrollBarsShown(true, false, true, false);
    viewport_.setViewedComponent(&itemsContainer_, false);
    viewport_.getVerticalScrollBar().setAutoHide(false);

    items_ = {
        { "signal_generator", "Signal Generator", "1 kHz sine at 0 dB" },
        { "channel", "Channel", "Main signal path" },
        { "output", "Output", "Stereo destination" },
        { "group", "Group", "Mix bus" },
        { "position", "Position", "Performer setup" },
        { "effect", "Effect", "Processing node" }
    };

    for (const auto& item : items_) {
        auto comp = std::make_unique<ItemComponent>(*this, item);
        itemsContainer_.addAndMakeVisible(*comp);
        itemComponents_.push_back(std::move(comp));
    }

    layoutItems(0);
}

void NodeLibraryComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto background = hasTheme_ ? toColour(theme_.background).darker(0.2F) : juce::Colours::darkslategrey;
    g.setColour(background);
    g.fillRoundedRectangle(bounds, 12.0F);

    if (hasTheme_) {
        auto outline = toColour(theme_.accent).withAlpha(0.25F);
        g.setColour(outline);
        g.drawRoundedRectangle(bounds.reduced(0.5F), 12.0F, 1.5F);
    }
}

void NodeLibraryComponent::resized() {
    auto area = getLocalBounds().reduced(16);

    auto headingArea = area.removeFromTop(34);
    heading_.setBounds(headingArea);

    area.removeFromTop(4);
    hint_.setBounds(area.removeFromTop(22));

    area.removeFromTop(12);
    viewport_.setBounds(area);

    const auto contentWidth = std::max(0, viewport_.getLocalBounds().getWidth());
    layoutItems(contentWidth);
}

void NodeLibraryComponent::setTheme(const ui::UiTheme& theme) {
    theme_ = theme;
    hasTheme_ = true;

    const auto primary = toColour(theme.textPrimary);
    const auto secondary = primary.withAlpha(0.7F);

    heading_.setColour(juce::Label::textColourId, primary);
    hint_.setColour(juce::Label::textColourId, secondary);

    for (auto& comp : itemComponents_) {
        if (comp != nullptr) {
            comp->repaint();
        }
    }

    repaint();
}

NodeLibraryComponent::ItemComponent::ItemComponent(NodeLibraryComponent& owner, Item item)
    : owner_(owner)
    , item_(std::move(item)) {
    title_.setText(item_.title, juce::dontSendNotification);
    title_.setJustificationType(juce::Justification::centredLeft);
    title_.setFont(juce::Font(juce::FontOptions { 15.0F, juce::Font::bold }));
    title_.setInterceptsMouseClicks(false, false);

    subtitle_.setText(item_.subtitle, juce::dontSendNotification);
    subtitle_.setJustificationType(juce::Justification::centredLeft);
    subtitle_.setFont(juce::Font(juce::FontOptions { 12.0F }));
    subtitle_.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(title_);
    addAndMakeVisible(subtitle_);
}

void NodeLibraryComponent::ItemComponent::paint(juce::Graphics& g) {
    const auto background = owner_.hasTheme_
        ? toColour(owner_.theme_.background).darker(isHovering_ ? 0.35F : 0.25F)
        : juce::Colours::dimgrey;

    g.setColour(background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0F);

    if (owner_.hasTheme_) {
        auto outline = toColour(owner_.theme_.accent).withAlpha(isHovering_ ? 0.5F : 0.25F);
        g.setColour(outline);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5F), 10.0F, 1.4F);
    }
}

void NodeLibraryComponent::ItemComponent::resized() {
    auto area = getLocalBounds().reduced(12);
    title_.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    subtitle_.setBounds(area.removeFromTop(18));
}

void NodeLibraryComponent::ItemComponent::mouseEnter(const juce::MouseEvent&) {
    isHovering_ = true;
    repaint();
}

void NodeLibraryComponent::ItemComponent::mouseExit(const juce::MouseEvent&) {
    isHovering_ = false;
    repaint();
}

void NodeLibraryComponent::ItemComponent::mouseDrag(const juce::MouseEvent& event) {
    if (!hasStartedDrag_ && event.getDistanceFromDragStart() > 4.0F) {
        hasStartedDrag_ = true;
        owner_.beginDragForItem(item_, this);
    }
}

void NodeLibraryComponent::ItemComponent::mouseUp(const juce::MouseEvent&) {
    hasStartedDrag_ = false;
}

void NodeLibraryComponent::beginDragForItem(const Item& item, juce::Component* sourceComponent) {
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
        juce::var description(item.id);
        container->startDragging(description, sourceComponent);
    }
}

void NodeLibraryComponent::layoutItems(int availableWidth) {
    const int contentWidth = std::max(availableWidth, 0);
    const int rowHeight = 64;
    const int rowSpacing = 8;

    int y = 0;
    for (auto& compPtr : itemComponents_) {
        if (compPtr == nullptr) {
            continue;
        }
        compPtr->setBounds(0, y, contentWidth, rowHeight);
        y += rowHeight + rowSpacing;
    }
    if (!itemComponents_.empty()) {
        y -= rowSpacing;
    }

    const int minHeight = viewport_.getLocalBounds().getHeight();
    itemsContainer_.setSize(contentWidth, std::max(y, minHeight));
}

} // namespace broadcastmix::app
