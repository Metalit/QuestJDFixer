#include "main.hpp"
#include "utils.hpp"

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "HMUI/AnimatedSwitchView.hpp"
#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RectTransform_Axis.hpp"

using namespace QuestUI;
using namespace HMUI;

GlobalNamespace::IDifficultyBeatmap* currentBeatmap = nullptr;
Values currentLevelValues = {};
float lastSpeed = 0;
float currentNps = 0;

Preset currentAppliedValues;
struct Indicator {
    std::string id = "";
    int idx = -1;
    bool which = 1;
}; Indicator lastPreset;
bool hasLevelPreset;

Preset currentModifiedValues;

#pragma region ui
static constexpr int maxFittingConditions = 3;

SliderSetting *durationSlider, *distanceSlider, *njsSlider, *minBoundSlider, *maxBoundSlider;
ClickableText *distanceText, *durationText, *njsText;
UnityEngine::UI::Toggle *njsToggle, *useDefaultsToggle, *useBoundsToggle, *levelSaveToggle;
UnityEngine::UI::Button *removeButton, *leftButton, *rightButton, *minusButton;
IncrementSetting *presetIncrement;
HMUI::SimpleTextDropdown *durDistDropdown;
TMPro::TextMeshProUGUI *levelStatusText;

SafePtrUnity<UnityEngine::GameObject> settingsGO;
UnityEngine::GameObject *mainParent, *presetsParent, *conditionsParent, *boundsParent;

template<BeatSaberUI::HasTransform P>
SliderSetting* CreateIncrementSlider(P parent, std::string name, float value, float increment, float min, float max, auto callback, float width = 40) {
    auto slider = BeatSaberUI::CreateSliderSetting(parent, name, 0.01, value, min, max, 0, callback);

    ((UnityEngine::RectTransform*) slider->get_transform())->set_sizeDelta({0, 8});

    auto transform = (UnityEngine::RectTransform*) slider->slider->get_transform();
    transform->set_anchoredPosition({-6, 0});
    // transform->set_anchoredPosition({14 - width/2, 0});
    transform->set_sizeDelta({width, 0});

    auto leftButton = BeatSaberUI::CreateUIButton(transform, "", "DecButton", {-width/2 - 2, 0}, {6, 8}, [slider = slider->slider, increment](){
        float newValue = slider->get_value() - increment;
        slider->SetNormalizedValue(slider->NormalizeValue(newValue));
    });
    auto rightButton = QuestUI::BeatSaberUI::CreateUIButton(transform, "", "IncButton", {width/2 + 2, 0}, {8, 8}, [slider = slider->slider, increment](){
        float newValue = slider->get_value() + increment;
        slider->SetNormalizedValue(slider->NormalizeValue(newValue));
    });

    return slider;
}

template<BeatSaberUI::HasTransform P>
SliderSetting* ReparentSlider(SliderSetting* slider, P parent) {
    auto newSlider = slider->slider->get_gameObject()->AddComponent<SliderSetting*>();
    newSlider->slider = slider->slider;
    newSlider->FormatString = slider->FormatString;
    int steps = slider->slider->get_numberOfSteps();
    auto callback = std::move(slider->OnValueChange);
    slider->OnValueChange = nullptr;
    newSlider->Setup(slider->slider->get_minValue(), slider->slider->get_maxValue(), slider->get_value(), 1, slider->timerResetValue, std::move(callback));
    newSlider->isInt = slider->isInt;
    newSlider->slider->set_numberOfSteps(steps);
    auto transform = newSlider->slider->get_transform();
    auto object = transform->GetParent()->get_gameObject();
    transform->SetParent(parent->get_transform(), false);
    UnityEngine::Object::Destroy(object);
    return newSlider;
}

void SetSliderBounds(SliderSetting* slider, float min, float max, float increment) {
    slider->isInt = abs(increment - round(increment)) < 0.000001;
    float value = slider->get_value();
    slider->slider->set_minValue(min);
    slider->slider->set_maxValue(max);
    slider->slider->set_numberOfSteps(((max - min) / increment) + 1);
    slider->set_value(value);
    slider->text->set_text(slider->TextForValue(slider->get_value()));
}

template<BeatSaberUI::HasTransform P, typename C = std::nullptr_t>
inline auto CreateCenteredText(P parent, std::string text, C callback = nullptr) {
    auto retText = [&]() {
        if constexpr(std::is_same_v<C, std::nullptr_t>)
            return BeatSaberUI::CreateText(parent, text, false);
        else
            return BeatSaberUI::CreateClickableText(parent, text, false, callback);
    }();
    retText->set_alignment(TMPro::TextAlignmentOptions::Center);
    retText->get_rectTransform()->set_sizeDelta({0, 0});
    return retText;
}

template<BeatSaberUI::HasTransform P>
inline UnityEngine::UI::Button* CreateSmallButton(P parent, std::string text, auto callback, std::string hint = "") {
    auto button = BeatSaberUI::CreateUIButton(parent, text, callback);
    static ConstString contentName("Content");
    UnityEngine::Object::Destroy(button->get_transform()->Find(contentName)->template GetComponent<UnityEngine::UI::LayoutElement*>());
    if(!hint.empty()) BeatSaberUI::AddHoverHint(button, hint);
    return button;
}

template<BeatSaberUI::HasTransform P>
inline HMUI::SimpleTextDropdown* CreateDropdownEnum(P parent, std::string name, int currentValue, const std::vector<std::string> dropdownStrings, auto intCallback, float width = 40) {
    std::vector<StringW> dropdownStringWs(dropdownStrings.begin(), dropdownStrings.end());
    auto object = BeatSaberUI::CreateDropdown(parent, name, dropdownStringWs[currentValue], dropdownStringWs,
        [dropdownStrings, intCallback](StringW value) {
            for(int i = 0; i < dropdownStrings.size(); i++) {
                if(value == dropdownStrings[i]) {
                    intCallback(i);
                    break;
                }
            }
        }
    );
    auto transform = (UnityEngine::RectTransform*) object->get_transform();
    transform->GetParent()->template GetComponent<::UnityEngine::UI::LayoutElement*>()->set_preferredHeight(7);
    transform->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, width);
    return object;
}

template<::QuestUI::BeatSaberUI::HasTransform P, class CV>
inline HMUI::SimpleTextDropdown* CreateNonSettingDropdownEnum(P parent, ConfigUtils::ConfigValue<CV>& configValue, int currentValue, const std::vector<std::string> dropdownStrings, auto intCallback) {
    auto object = CreateDropdownEnum(parent, configValue.GetName(), currentValue, dropdownStrings, intCallback);
    if(!configValue.GetHoverHint().empty())
        ::QuestUI::BeatSaberUI::AddHoverHint(object, configValue.GetHoverHint());
    return object;
}

template<BeatSaberUI::HasTransform P>
inline void ReparentDropdown(HMUI::SimpleTextDropdown* dropdown, P parent) {
    auto dropdownParent = dropdown->get_transform()->GetParent();
    dropdown->get_transform()->SetParent(parent->get_transform(), false);
    UnityEngine::Object::Destroy(dropdownParent->get_gameObject());
}

template<BeatSaberUI::HasTransform P, class CV>
inline UnityEngine::UI::Toggle* CreateNonSettingToggle(P parent, ConfigUtils::ConfigValue<CV>& configValue, bool currentValue, auto callback) {
    auto object = BeatSaberUI::CreateToggle(parent, configValue.GetName(), currentValue, callback);
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(object->get_gameObject(), configValue.GetHoverHint());
    ((UnityEngine::RectTransform*) object->get_transform())->set_sizeDelta({18, 8});
    return object;
}

template<BeatSaberUI::HasTransform P>
inline UnityEngine::UI::Toggle* AddConfigValueToggle(P parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = CreateNonSettingToggle(parent, configValue, configValue.GetValue(),
        [&configValue, callback](bool value) {
            configValue.SetValue(value);
            callback(value);
        }
    );
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(object, configValue.GetHoverHint());
    return object;
}

inline void InstantSetToggle(UnityEngine::UI::Toggle* toggle, bool value) {
    if(toggle->m_IsOn == value)
        return;
    toggle->m_IsOn = value;
    auto animatedSwitch = toggle->GetComponent<HMUI::AnimatedSwitchView*>();
    animatedSwitch->HandleOnValueChanged(value);
    animatedSwitch->switchAmount = value;
    animatedSwitch->LerpPosition(value);
    animatedSwitch->LerpColors(value, animatedSwitch->highlightAmount, animatedSwitch->disabledAmount);
}

template<class T>
inline void SetActive(T* component, bool active) {
    component->get_gameObject()->SetActive(active);
}

inline void SetText(TMPro::TextMeshProUGUI* text, float main, float secondary) {
    if(abs(main - secondary) < 0.005 || secondary == 0)
        text->set_text(string_format("<#5ec462>%.1f", secondary));
    else
        text->set_text(string_format("<#ff6969>%.1f <size=75%%><#5ec462>(%.1f)", main, secondary));
}

inline void ReplaceAll(std::string& text, std::string_view repl, std::string_view fill) {
    auto pos = text.find(repl);
    while(pos != std::string::npos) {
        text.replace(pos, repl.size(), fill);
        pos = text.find(repl);
    }
}

void Highlight(TMPro::TextMeshProUGUI* text) {
    std::string str = text->get_text();
    ReplaceAll(str, "#5ec462", "#a9c7aa");
    ReplaceAll(str, "#ff6969", "#ffb3b3");
    text->set_text(str);
}
void Unhighlight(TMPro::TextMeshProUGUI* text) {
    std::string str = text->get_text();
    ReplaceAll(str, "#a9c7aa", "#5ec462");
    ReplaceAll(str, "#ffb3b3", "#ff6969");
    text->set_text(str);
}

void UpdateTexts() {
    SetText(durationText, currentAppliedValues.GetDuration(), currentLevelValues.halfJumpDuration);
    SetText(distanceText, currentAppliedValues.GetDistance(), currentLevelValues.halfJumpDistance);
    SetText(njsText, currentAppliedValues.GetNJS(), currentLevelValues.njs);
}

void UpdateLevelSaveStatus() {
    InstantSetToggle(levelSaveToggle, currentAppliedValues.GetIsLevelPreset() && currentAppliedValues.GetAsLevelPreset().Active);
    levelStatusText->set_text(hasLevelPreset ? "Settings Exist" : "No Settings Exist");
    removeButton->set_interactable(hasLevelPreset);
}

void UpdateMainUI() {
    UpdateTexts();
    SetActive(durationSlider, currentAppliedValues.GetUseDuration());
    SetActive(distanceSlider, !currentAppliedValues.GetUseDuration());
    if(currentAppliedValues.GetUseDuration())
        durationSlider->set_value(currentAppliedValues.GetMainValue());
    else
        distanceSlider->set_value(currentAppliedValues.GetMainValue());
    InstantSetToggle(njsToggle, currentAppliedValues.GetOverrideNJS());
    SetActive(njsSlider, currentAppliedValues.GetOverrideNJS());
    njsSlider->set_value(currentAppliedValues.GetNJS());
    UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
    UpdateLevelSaveStatus();
}

void UpdateConditions() {
    auto transform = conditionsParent->get_transform();
    int children = transform->GetChildCount();
    for(int i = 0; i < children; i++) {
        if(i < currentModifiedValues.GetConditionCount() * 4) {
            auto cond = currentModifiedValues.GetCondition(i / 4);
            auto child = transform->GetChild(i);
            switch(i % 4) {
            case 0:
                if(i != 0)
                    child->GetComponent<HMUI::SimpleTextDropdown*>()->SelectCellWithIdx(cond.AndOr);
                break;
            case 1:
                child->GetComponent<HMUI::SimpleTextDropdown*>()->SelectCellWithIdx(cond.Type);
                break;
            case 2:
                child->GetComponent<HMUI::SimpleTextDropdown*>()->SelectCellWithIdx(cond.Comparison);
                break;
            case 3: {
                auto slider = child->GetChild(0)->GetComponent<SliderSetting*>();
                slider->set_value(cond.Value);
                if(cond.Type == 2)
                    SetSliderBounds(slider, 0, 1000, 10);
                else
                    SetSliderBounds(slider, 0, 30, 0.1);
                break;
            }}
            SetActive(child, true);
        } else
            SetActive(transform->GetChild(i), false);
    }
    UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate(presetsParent->GetComponent<UnityEngine::RectTransform*>());
}

void UpdatePresetControl() {
    auto idx = currentModifiedValues.GetConditionPresetIndex();
    leftButton->set_interactable(idx > 0);
    rightButton->set_interactable(idx != -1 && idx < getModConfig().Presets.GetValue().size() - 1);
    minusButton->set_interactable(idx != -1);
    std::string str = std::to_string((int) presetIncrement->CurrentValue);
    if(presetIncrement->CurrentValue == getModConfig().Presets.GetValue().size() + 1)
        str = "Default";
    presetIncrement->Text->SetText(str);
    auto buttons = presetIncrement->Text->get_transform()->GetParent()->GetComponentsInChildren<UnityEngine::UI::Button*>();
    buttons.First()->set_interactable(presetIncrement->CurrentValue > presetIncrement->MinValue || !presetIncrement->HasMin);
    buttons.Last()->set_interactable(presetIncrement->CurrentValue < presetIncrement->MaxValue || !presetIncrement->HasMax);
}

void UpdatePresetUI() {
    UpdateConditions();
    UpdatePresetControl();
    InstantSetToggle(useDefaultsToggle, currentModifiedValues.GetUseDefaults());
    durDistDropdown->SelectCellWithIdx(currentModifiedValues.GetUseDuration());
    InstantSetToggle(useBoundsToggle, currentModifiedValues.GetUseBounds());
    boundsParent->SetActive(currentModifiedValues.GetUseBounds());
    minBoundSlider->set_value(currentModifiedValues.GetBoundMin());
    maxBoundSlider->set_value(currentModifiedValues.GetBoundMax());
}

bool UpdatePreset();

void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation) {
    if(firstActivation) {

        auto mainVertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject);
        mainVertical->set_childControlHeight(false);
        mainVertical->set_childForceExpandHeight(false);
        mainVertical->set_childForceExpandWidth(true);
        mainVertical->set_spacing(0.5);

        mainParent = mainVertical->get_gameObject();

        // raise up container
        mainVertical->get_rectTransform()->set_anchoredPosition({0, 31});

        auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(mainVertical);
        horizontal->set_spacing(5);

        auto titleText = BeatSaberUI::CreateText(horizontal->get_transform(), "Main Settings");
        titleText->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(15);
        titleText->set_alignment(TMPro::TextAlignmentOptions::MidlineLeft);

        CreateSmallButton(horizontal, "Presets", []() {
            mainParent->SetActive(false);
            presetsParent->SetActive(true);
        }, "Modify conditional presets");

        auto enableToggle = AddConfigValueToggle(horizontal, getModConfig().Disable, [](bool _) {
            UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
        });
        enableToggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(30);

        auto spaced = BeatSaberUI::CreateGridLayoutGroup(mainVertical);
        spaced->set_constraint(UnityEngine::UI::GridLayoutGroup::Constraint::FixedRowCount);
        spaced->set_constraintCount(2);
        spaced->set_cellSize({30, 8});
        spaced->get_rectTransform()->set_sizeDelta({0, 12});
        spaced->set_spacing({0, -2});

        UnityEngine::Color labelColor(0.3, 0.7, 1, 1);
        CreateCenteredText(spaced, "Duration")->set_color(labelColor);
        CreateCenteredText(spaced, "Distance")->set_color(labelColor);
        CreateCenteredText(spaced, "NJS")->set_color(labelColor);

        durationText = CreateCenteredText(spaced, "", []() {
            currentAppliedValues.SetDuration(currentLevelValues.halfJumpDuration);
            UpdateMainUI();
        });
        SetText(durationText, 0, 0);
        durationText->get_onPointerEnterEvent().addCallback(*[](UnityEngine::EventSystems::PointerEventData* _) { Highlight(durationText); });
        durationText->get_onPointerExitEvent().addCallback(*[](UnityEngine::EventSystems::PointerEventData* _) { Unhighlight(durationText); });

        distanceText = CreateCenteredText(spaced, "", []() {
            currentAppliedValues.SetDistance(currentLevelValues.halfJumpDistance);
            UpdateMainUI();
        });
        SetText(distanceText, 12, 23.1);
        distanceText->get_onPointerEnterEvent().addCallback(*[](UnityEngine::EventSystems::PointerEventData* _) { Highlight(distanceText); });
        distanceText->get_onPointerExitEvent().addCallback(*[](UnityEngine::EventSystems::PointerEventData* _) { Unhighlight(distanceText); });

        njsText = CreateCenteredText(spaced, "", []() {
            currentAppliedValues.SetNJS(currentLevelValues.njs);
            UpdateMainUI();
        });
        SetText(njsText, 3, 16);
        njsText->get_onPointerEnterEvent().addCallback(*[](UnityEngine::EventSystems::PointerEventData* _) { Highlight(njsText); });
        njsText->get_onPointerExitEvent().addCallback(*[](UnityEngine::EventSystems::PointerEventData* _) { Unhighlight(njsText); });

        for(int i = 0; i < spaced->get_transform()->GetChildCount(); i++)
            BeatSaberUI::AddHoverHint(spaced->get_transform()->GetChild(i), "The default for the level (green) and the applied (red) values. Click to set to the level default");

        durationSlider = CreateIncrementSlider(mainVertical, "Half Jump Duration", currentAppliedValues.GetMainValue(), 0.05, 0.1, 1.5, [](float value) {
            currentAppliedValues.SetMainValue(value);
            UpdateTexts();
        }, 50);
        distanceSlider = CreateIncrementSlider(mainVertical, "Half Jump Distance", currentAppliedValues.GetMainValue(), 0.1, 1, 30, [](float value) {
            currentAppliedValues.SetMainValue(value);
            UpdateTexts();
        }, 50);

        // njs toggle
        njsToggle = CreateNonSettingToggle(mainVertical, getModConfig().UseNJS, currentAppliedValues.GetOverrideNJS(), [](bool enabled) {
            SetActive(njsSlider, enabled);
            currentAppliedValues.SetOverrideNJS(enabled);
            UpdateScoreSubmission(enabled);
            UpdateTexts();
        });

        // njs slider
        njsSlider = CreateIncrementSlider(mainVertical, "", currentAppliedValues.GetNJS(), 0.1, 1, 30, [](float value) {
            currentAppliedValues.SetNJS(value);
            UpdateTexts();
        });
        njsSlider = ReparentSlider(njsSlider, njsToggle);
        ((UnityEngine::RectTransform*) njsSlider->slider->get_transform())->set_anchoredPosition({-25, 0});

        auto horizontal1 = BeatSaberUI::CreateHorizontalLayoutGroup(mainVertical);
        horizontal1->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
        horizontal1->set_childForceExpandWidth(false);
        horizontal1->set_spacing(1);

        bool presetActive = currentAppliedValues.GetIsLevelPreset() && currentAppliedValues.GetAsLevelPreset().Active;
        levelSaveToggle = BeatSaberUI::CreateToggle(horizontal1, "Level Specifc Settings", presetActive, [](bool enabled) {
            if(!currentBeatmap)
                return;
            auto levels = getModConfig().Levels.GetValue();
            std::string search = currentBeatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
            if(levels.count(search) == 0)
                levels[search] = currentAppliedValues.GetAsLevelPreset();
            levels[search].Active = enabled;
            getModConfig().Levels.SetValue(levels);
            if(UpdatePreset())
                UpdateMainUI();
        });
        BeatSaberUI::AddHoverHint(levelSaveToggle, "Use and change settings specifically for the currently selected level instead of a preset");
        levelSaveToggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50);

        levelStatusText = CreateCenteredText(horizontal1, hasLevelPreset ? "Settings Exist" : "No Settings Exist");
        levelStatusText->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(30);
        removeButton = CreateSmallButton(horizontal1, "X", []() {
            if(!currentBeatmap)
                return;
            auto levels = getModConfig().Levels.GetValue();
            auto iter = levels.find(currentBeatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID());
            if(iter == levels.end())
                return;
            levels.erase(iter);
            getModConfig().Levels.SetValue(levels);
            if(UpdatePreset())
                UpdateMainUI();
            else
                UpdateLevelSaveStatus();
        }, "Remove specific settings for this level");
        removeButton->set_interactable(hasLevelPreset);
        ((UnityEngine::RectTransform*) removeButton->get_transform())->set_sizeDelta({8, 8});

        auto horizontal2 = BeatSaberUI::CreateHorizontalLayoutGroup(mainVertical);
        horizontal2->set_spacing(6);

        AddConfigValueToggle(horizontal2, getModConfig().Practice)->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(42);
        AddConfigValueToggle(horizontal2, getModConfig().Half)->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(42);

        auto presetsVertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject);
        presetsVertical->set_childControlHeight(false);
        presetsVertical->set_childForceExpandHeight(false);
        presetsVertical->set_childForceExpandWidth(true);
        presetsVertical->set_spacing(0.5);

        presetsParent = presetsVertical->get_gameObject();

        // raise up container
        presetsVertical->get_rectTransform()->set_anchoredPosition({0, 31});

        auto horizontal3 = BeatSaberUI::CreateHorizontalLayoutGroup(presetsVertical);
        horizontal3->set_spacing(1);

        CreateSmallButton(horizontal3, "Back", []() {
            mainParent->SetActive(true);
            presetsParent->SetActive(false);
        });

        auto presetLabel = BeatSaberUI::CreateText(horizontal3->get_transform(), "Presets");
        presetLabel->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(20);
        presetLabel->set_alignment(TMPro::TextAlignmentOptions::MidlineLeft);

        leftButton = CreateSmallButton(horizontal3, "<", []() {
            if(currentModifiedValues.ShiftBackward()) {
                presetIncrement->CurrentValue--;
                UpdatePresetControl();
            }
        }, "Move preset up in priority");
        rightButton = CreateSmallButton(horizontal3, ">", []() {
            if(currentModifiedValues.ShiftForward()) {
                presetIncrement->CurrentValue++;
                UpdatePresetControl();
            }
        }, "Move preset down in priority");

        int presetNum = getModConfig().Presets.GetValue().size();
        presetIncrement = BeatSaberUI::CreateIncrementSetting(gameObject, "", 0, 1, presetNum + 1, 1, presetNum + 1, [](float preset) {
            preset--; // goes 1, 2, 3... as displayed and the last is the main config
            if(preset == getModConfig().Presets.GetValue().size())
                currentModifiedValues = Preset(currentLevelValues);
            else
                currentModifiedValues = Preset(preset, currentLevelValues);
            UpdatePresetUI();
        });
        SetButtons(presetIncrement);
        auto incrementObject = presetIncrement->get_transform()->GetChild(1);
        incrementObject->SetParent(horizontal3->get_transform());
        incrementObject->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(30);

        minusButton = CreateSmallButton(horizontal3, "-", []() {
            int idx = currentModifiedValues.GetConditionPresetIndex();
            if(idx == -1)
                return;
            auto presets = getModConfig().Presets.GetValue();
            presets.erase(presets.begin() + idx);
            getModConfig().Presets.SetValue(presets);
            presetIncrement->MaxValue = presets.size() + 1;
            presetIncrement->UpdateValue();
            if(UpdatePreset())
                UpdateMainUI();
        }, "Delete current preset");
        CreateSmallButton(horizontal3, "+", []() {
            auto presets = getModConfig().Presets.GetValue();
            presets.emplace_back();
            getModConfig().Presets.SetValue(presets);
            presetIncrement->CurrentValue = presets.size();
            presetIncrement->MaxValue = presets.size() + 1;
            presetIncrement->UpdateValue();
            if(UpdatePreset())
                UpdateMainUI();
        }, "Add new preset");

        auto spaced2 = BeatSaberUI::CreateGridLayoutGroup(presetsVertical);
        spaced2->set_constraint(UnityEngine::UI::GridLayoutGroup::Constraint::FixedColumnCount);
        spaced2->set_constraintCount(4);
        spaced2->set_cellSize({22.5, 8});
        spaced2->set_spacing({0, 0.5});
        spaced2->get_gameObject()->AddComponent<UnityEngine::UI::ContentSizeFitter*>()->set_verticalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
        conditionsParent = spaced2->get_gameObject();

        std::vector<std::string> options1 = {"And", "Or", "Delete"};
        std::vector<std::string> options2 = {"NPS", "NJS", "BPM"};
        std::vector<std::string> options3 = {"Under", "Over"};
        for(int i = 0; i < maxFittingConditions; i++) {
            if(i == 0) {
                auto horizontal4 = BeatSaberUI::CreateHorizontalLayoutGroup(spaced2);
                horizontal4->set_childControlWidth(true);
                CreateSmallButton(horizontal4, "+", []() {
                    if(currentModifiedValues.GetConditionCount() < maxFittingConditions)
                        currentModifiedValues.SetCondition({}, maxFittingConditions);
                    UpdateConditions();
                }, "Add new condition for the current preset");
                CreateCenteredText(horizontal4, "On")->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(12);
            } else {
                ReparentDropdown(CreateDropdownEnum(presetsVertical, "", currentModifiedValues.GetCondition(i).AndOr, options1, [i](int option) {
                    if(option != 2) {
                        auto cond = currentModifiedValues.GetCondition(i);
                        cond.AndOr = option;
                        currentModifiedValues.SetCondition(cond, i);
                        if(UpdatePreset())
                            UpdateMainUI();
                    } else {
                        currentModifiedValues.RemoveCondition(i);
                        UpdateConditions();
                    }
                }, 22), spaced2);
            }
            ReparentDropdown(CreateDropdownEnum(presetsVertical, "", currentModifiedValues.GetCondition(i).Type, options2, [i](int option) {
                auto cond = currentModifiedValues.GetCondition(i);
                cond.Type = option;
                currentModifiedValues.SetCondition(cond, i);
                UpdateConditions();
                if(UpdatePreset())
                    UpdateMainUI();
            }, 22), spaced2);
            ReparentDropdown(CreateDropdownEnum(presetsVertical, "", currentModifiedValues.GetCondition(i).Comparison, options3, [i](int option) {
                auto cond = currentModifiedValues.GetCondition(i);
                cond.Comparison = option;
                currentModifiedValues.SetCondition(cond, i);
                if(UpdatePreset())
                    UpdateMainUI();
            }, 22), spaced2);
            auto slider = BeatSaberUI::CreateSliderSetting(presetsVertical, "", 1, currentModifiedValues.GetCondition(i).Value, 0, 1000, 0, [i](float value) {
                auto cond = currentModifiedValues.GetCondition(i);
                cond.Value = value;
                currentModifiedValues.SetCondition(cond, i);
                if(UpdatePreset())
                    UpdateMainUI();
            });
            auto rect = (UnityEngine::RectTransform*) slider->slider->get_transform();
            rect->set_sizeDelta({32, 0});
            rect->set_anchoredPosition({9.5, 0});
            auto wrapper = UnityEngine::GameObject::New_ctor("JDFixerSliderWrapper", {(System::Type*) csTypeOf(UnityEngine::RectTransform*)});
            wrapper->get_transform()->SetParent(spaced2->get_transform(), false);
            ReparentSlider(slider, wrapper);
        }

        useDefaultsToggle = CreateNonSettingToggle(presetsVertical, getModConfig().AutoDef, currentModifiedValues.GetUseDefaults(), [](bool enabled) {
            currentModifiedValues.SetUseDefaults(enabled);
            if(!currentBeatmap)
                return;
            currentAppliedValues.UpdateLevel(currentLevelValues);
            UpdateMainUI();
        });

        std::vector<std::string> mainValueOptions = {"Distance", "Duration"};
        durDistDropdown = CreateNonSettingDropdownEnum(presetsVertical, getModConfig().UseDuration, currentModifiedValues.GetUseDuration(), mainValueOptions, [](int option) {
            currentModifiedValues.SetUseDuration(option);
            UpdateMainUI();
        });

        useBoundsToggle = CreateNonSettingToggle(presetsVertical, getModConfig().BoundJD, currentModifiedValues.GetUseBounds(), [](bool enabled) {
            currentModifiedValues.SetUseBounds(enabled);
            boundsParent->SetActive(enabled);
            UpdateTexts();
        });

        boundsParent = UnityEngine::GameObject::New_ctor("JDFixerBoundsParent");
        boundsParent->AddComponent<UnityEngine::RectTransform*>()->set_sizeDelta({90, 8});
        boundsParent->get_transform()->SetParent(presetsVertical->get_transform(), false);
        auto horizontal5 = BeatSaberUI::CreateHorizontalLayoutGroup(boundsParent);
        horizontal5->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredHeight(8);
        horizontal5->set_childControlWidth(false);
        horizontal5->get_rectTransform()->set_anchoredPosition({4, 0});

        minBoundSlider = ReparentSlider(CreateIncrementSlider(presetsVertical, "Min", currentModifiedValues.GetBoundMin(), 0.1, 0, 30, [](float value) {
            currentModifiedValues.SetBoundMin(value);
            UpdateTexts();
        }, 34), horizontal5);
        CreateCenteredText(horizontal5, "to");
        maxBoundSlider = ReparentSlider(CreateIncrementSlider(presetsVertical, "Max", currentModifiedValues.GetBoundMax(), 0.1, 1, 30, [](float value) {
            currentModifiedValues.SetBoundMax(value);
            UpdateTexts();
        }, 34), horizontal5);

        UpdateMainUI();
        UpdatePresetUI();
    }
    mainParent->SetActive(true);
    presetsParent->SetActive(false);

    settingsGO = gameObject;
}
#pragma endregion

bool UpdatePreset() {
    if(!currentBeatmap)
        return false;
    std::string search = currentBeatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(search);
    if(iter != map.end()) {
        hasLevelPreset = true;
        if(iter->second.Active) {
            // only reset when preset changes
            if(lastPreset.which != 0 || lastPreset.id != search) {
                currentAppliedValues = Preset(search, currentLevelValues);
                lastPreset.id = search;
                lastPreset.which = 0;
                return true;
            }
            return false;
        }
    } else
        hasLevelPreset = false;
    float bpm = GetBPM(currentBeatmap);
    auto presets = getModConfig().Presets.GetValue();
    for(int i = 0; i < presets.size(); i++) {
        if(ConditionMet(presets[i], currentLevelValues.njs, currentNps, bpm)) {
            if(lastPreset.which != 1 || lastPreset.idx != i) {
                currentAppliedValues = Preset(i, currentLevelValues);
                lastPreset.idx = i;
                lastPreset.which = 1;
                return true;
            }
            return false;
        }
    }
    if(lastPreset.which != 1 || lastPreset.idx != -1) {
        currentAppliedValues = Preset(currentLevelValues);
        lastPreset.idx = -1;
        lastPreset.which = 1;
        return true;
    }
    return false;
}

void UpdateLevel(GlobalNamespace::IDifficultyBeatmap* beatmap, float speed) {
    if(!currentBeatmap)
        lastSpeed = speed;
    if(currentBeatmap == beatmap)
        return;
    // beatmap being non null here signifies that it changed
    if(beatmap)
        currentBeatmap = beatmap;
    currentLevelValues = GetLevelDefaults(currentBeatmap, speed);
    bool currentValuesChanged = UpdatePreset();
    if(currentValuesChanged)
        UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
    else if(beatmap || lastSpeed != speed)
        currentAppliedValues.UpdateLevel(currentLevelValues);
    else // if nothing was changed
        return;
    if(settingsGO)
        UpdateMainUI();
    lastSpeed = speed;
}

void UpdateNotesPerSecond(float nps) {
    currentNps = nps;
    UpdateLevel(nullptr, lastSpeed);
}

LevelPreset GetAppliedValues() {
    return currentAppliedValues.GetAsLevelPreset();
}
