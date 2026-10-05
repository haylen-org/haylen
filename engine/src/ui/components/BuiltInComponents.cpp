#include "ui/components/BuiltInComponents.hpp"

#include "haylen/ui/ComponentRegistry.hpp"
#include "ui/components/buttons/Button.hpp"
#include "ui/components/buttons/Chip.hpp"
#include "ui/components/buttons/ImageButton.hpp"
#include "ui/components/buttons/MenuButton.hpp"
#include "ui/components/buttons/Popover.hpp"
#include "ui/components/choices/Checkbox.hpp"
#include "ui/components/choices/Combo.hpp"
#include "ui/components/choices/RadioGroup.hpp"
#include "ui/components/choices/SegmentedControl.hpp"
#include "ui/components/choices/Toggle.hpp"
#include "ui/components/collections/List.hpp"
#include "ui/components/collections/SlotGrid.hpp"
#include "ui/components/collections/Table.hpp"
#include "ui/components/collections/Tree.hpp"
#include "ui/components/containers/Accordion.hpp"
#include "ui/components/containers/Card.hpp"
#include "ui/components/containers/Carousel.hpp"
#include "ui/components/containers/Column.hpp"
#include "ui/components/containers/Divider.hpp"
#include "ui/components/containers/FormField.hpp"
#include "ui/components/containers/Grid.hpp"
#include "ui/components/containers/Panel.hpp"
#include "ui/components/containers/Row.hpp"
#include "ui/components/containers/SafeArea.hpp"
#include "ui/components/containers/Scroll.hpp"
#include "ui/components/containers/Spacer.hpp"
#include "ui/components/containers/Splitter.hpp"
#include "ui/components/containers/Stack.hpp"
#include "ui/components/containers/Tabs.hpp"
#include "ui/components/indicators/Avatar.hpp"
#include "ui/components/indicators/Badge.hpp"
#include "ui/components/indicators/BusyIndicator.hpp"
#include "ui/components/indicators/CircularProgress.hpp"
#include "ui/components/indicators/Icon.hpp"
#include "ui/components/indicators/Image.hpp"
#include "ui/components/indicators/Progress.hpp"
#include "ui/components/indicators/StatusIndicator.hpp"
#include "ui/components/inputs/ColorField.hpp"
#include "ui/components/inputs/FilterField.hpp"
#include "ui/components/inputs/KeyCapture.hpp"
#include "ui/components/inputs/NumberField.hpp"
#include "ui/components/inputs/PlayArea.hpp"
#include "ui/components/inputs/RangeSlider.hpp"
#include "ui/components/inputs/SecretField.hpp"
#include "ui/components/inputs/Slider.hpp"
#include "ui/components/inputs/Stepper.hpp"
#include "ui/components/inputs/TextArea.hpp"
#include "ui/components/inputs/TextField.hpp"
#include "ui/components/overlays/ContextMenu.hpp"
#include "ui/components/overlays/Dialog.hpp"
#include "ui/components/overlays/Toast.hpp"
#include "ui/components/overlays/Window.hpp"
#include "ui/components/settings/SettingsActions.hpp"
#include "ui/components/settings/SettingsForm.hpp"
#include "ui/components/settings/SettingsRow.hpp"
#include "ui/components/text/Alert.hpp"
#include "ui/components/text/EmptyState.hpp"
#include "ui/components/text/Label.hpp"
#include "ui/components/text/PageHeader.hpp"
#include "ui/components/text/RichText.hpp"
#include "ui/components/text/SectionTitle.hpp"
#include "ui/components/touch/TouchButton.hpp"
#include "ui/components/touch/TouchStick.hpp"

namespace haylen::ui {

void BuiltInComponents::registerAll(ComponentRegistry& registry) {
    registry.add<Column>("column");
    registry.add<Row>("row");
    registry.add<Grid>("grid");
    registry.add<Stack>("stack");
    registry.add<Scroll>("scroll");
    registry.add<Card>("card");
    registry.add<Panel>("panel");
    registry.add<Spacer>("spacer");
    registry.add<Divider>("divider");
    registry.add<Tabs>("tabs");
    registry.add<FormField>("formField");
    registry.add<Splitter>("splitter");
    registry.add<SafeArea>("safeArea");
    registry.add<Accordion>("accordion");
    registry.add<Carousel>("carousel");
    registry.add<Label>("label");
    registry.add<RichText>("richText");
    registry.add<PageHeader>("pageHeader");
    registry.add<SectionTitle>("sectionTitle");
    registry.add<EmptyState>("emptyState");
    registry.add<Alert>("alert");
    registry.add<Button>("button");
    registry.add<ImageButton>("imageButton");
    registry.add<Chip>("chip");
    registry.add<MenuButton>("menuButton");
    registry.add<Popover>("popover");
    registry.add<Checkbox>("checkbox");
    registry.add<Toggle>("toggle");
    registry.add<RadioGroup>("radioGroup");
    registry.add<Combo>("combo");
    registry.add<SegmentedControl>("segmentedControl");
    registry.add<TextField>("textField");
    registry.add<SecretField>("secretField");
    registry.add<TextArea>("textArea");
    registry.add<FilterField>("filterField");
    registry.add<NumberField>("numberField");
    registry.add<Slider>("slider");
    registry.add<RangeSlider>("rangeSlider");
    registry.add<Stepper>("stepper");
    registry.add<KeyCapture>("keyCapture");
    registry.add<ColorField>("colorField");
    registry.add<Badge>("badge");
    registry.add<StatusIndicator>("statusIndicator");
    registry.add<BusyIndicator>("busyIndicator");
    registry.add<Progress>("progress");
    registry.add<CircularProgress>("circularProgress");
    registry.add<Icon>("icon");
    registry.add<Image>("image");
    registry.add<Avatar>("avatar");
    registry.add<List>("list");
    registry.add<Tree>("tree");
    registry.add<Table>("table");
    registry.add<SlotGrid>("slotGrid");
    registry.add<SettingsForm>("settingsForm");
    registry.add<SettingsRow>("settingsRow");
    registry.add<SettingsActions>("settingsActions");
    registry.add<Dialog>("dialog");
    registry.add<Toast>("toast");
    registry.add<Window>("window");
    registry.add<ContextMenu>("contextMenu");
    registry.add<TouchStick>("touchStick");
    registry.add<TouchButton>("touchButton");
    registry.add<PlayArea>("playArea");
}

} // namespace haylen::ui
