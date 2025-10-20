#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <ui/UiTheme.h>

#include <vector>

namespace broadcastmix::app {

class NodeLibraryComponent : public juce::Component {
public:
    struct Item {
        juce::String id;
        juce::String title;
        juce::String subtitle;
    };

    NodeLibraryComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setTheme(const ui::UiTheme& theme);

private:
    class ItemComponent : public juce::Component {
    public:
        ItemComponent(NodeLibraryComponent& owner, Item item);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        NodeLibraryComponent& owner_;
        Item item_;
        juce::Label title_;
        juce::Label subtitle_;
        bool isHovering_ { false };
        bool hasStartedDrag_ { false };
    };

    void beginDragForItem(const Item& item, juce::Component* sourceComponent);

    ui::UiTheme theme_;
    bool hasTheme_ { false };
    juce::Label heading_ { "heading", "Node Library" };
    juce::Label hint_ { "hint", "Drag items onto the canvas" };
    juce::Viewport viewport_;
    juce::Component itemsContainer_;
    std::vector<Item> items_;
    std::vector<std::unique_ptr<ItemComponent>> itemComponents_;

    void layoutItems(int availableWidth);
};

} // namespace broadcastmix::app
