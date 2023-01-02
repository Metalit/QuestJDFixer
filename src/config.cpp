#include "main.hpp"
#include "utils.hpp"
#include "config.hpp"

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "HMUI/AnimatedSwitchView.hpp"
#include "UnityEngine/Rect.hpp"

using namespace QuestUI;
using namespace HMUI;

GlobalNamespace::IDifficultyBeatmap* currentBeatmap = nullptr;
Values currentLevelValues = {};

Preset currentAppliedValues;
struct Indicator {
    std::string id = "";
    int idx = -1;
    bool which = 1;
}; Indicator lastPreset;

Preset currentModifiedValues;

#pragma region ui
SliderSetting *durationSlider, *distanceSlider, *njsSlider;
ClickableText *distanceText, *durationText, *njsText;
UnityEngine::UI::Toggle* njsToggle;
IncrementSetting *presetIncrement;

SafePtrUnity<UnityEngine::GameObject> settingsGO;
UnityEngine::GameObject *mainParent, *presetsParent, *boundsParent;

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
    newSlider->Setup(slider->slider->get_minValue(), slider->slider->get_maxValue(), slider->get_value(), 1, slider->timerResetValue, slider->OnValueChange);
    newSlider->isInt = slider->isInt;
    newSlider->slider->set_numberOfSteps(steps);
    auto transform = newSlider->slider->get_transform();
    auto object = transform->GetParent()->get_gameObject();
    transform->SetParent(parent->get_transform(), false);
    UnityEngine::Object::Destroy(object);
    return newSlider;
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
inline UnityEngine::UI::Button* CreateSmallButton(P parent, std::string text, auto callback) {
    auto button = BeatSaberUI::CreateUIButton(parent, text, callback);
    static ConstString contentName("Content");
    UnityEngine::Object::Destroy(button->get_transform()->Find(contentName)->template GetComponent<UnityEngine::UI::LayoutElement*>());
    return button;
}

template<::QuestUI::BeatSaberUI::HasTransform P, class CV>
inline ::HMUI::SimpleTextDropdown* CreateNonSettingDropdownEnum(P parent, ConfigUtils::ConfigValue<CV>& configValue, int currentValue, const std::vector<std::string> dropdownStrings, auto intCallback) {
    std::vector<StringW> dropdownStringWs(dropdownStrings.begin(), dropdownStrings.end());
    auto object = ::QuestUI::BeatSaberUI::CreateDropdown(parent, configValue.GetName(), dropdownStringWs[currentValue], dropdownStringWs,
        [dropdownStrings, intCallback](StringW value) {
            for(int i = 0; i < dropdownStrings.size(); i++) {
                if(value == dropdownStrings[i]) {
                    intCallback(i);
                    break;
                }
            }
        }
    );
    object->get_transform()->GetParent()->template GetComponent<::UnityEngine::UI::LayoutElement*>()->set_preferredHeight(7);
    if(!configValue.GetHoverHint().empty())
        ::QuestUI::BeatSaberUI::AddHoverHint(object, configValue.GetHoverHint());
    return object;
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
    return CreateNonSettingToggle(parent, configValue, configValue.GetValue(),
        [&configValue, callback](bool value) {
            configValue.SetValue(value);
            callback(value);
        }
    );
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
    if(abs(main - secondary) < 0.005)
        text->set_text(string_format("<#5ec462>%.1f", main));
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
}

void UpdatePresetUI() {

}

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
        horizontal->set_spacing(10);

        auto enableToggle = AddConfigValueToggle(horizontal, getModConfig().Disable, [](bool _) {
            UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
        });
        enableToggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50);

        CreateSmallButton(horizontal, "Presets", []() {
            mainParent->SetActive(false);
            presetsParent->SetActive(true);
        });

        auto spaced = BeatSaberUI::CreateGridLayoutGroup(mainVertical);
        spaced->set_constraint(UnityEngine::UI::GridLayoutGroup::Constraint::FixedRowCount);
        spaced->set_constraintCount(2);
        spaced->set_cellSize({30, 8});
        spaced->get_rectTransform()->set_sizeDelta({0, 12});
        spaced->set_spacing({0, -2});

        // static ConstString textsHoverHint("The default for the level (green) and the applied (red) values. Click to set to the level default");
        // BeatSaberUI::AddHoverHint(spaced, textsHoverHint);

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
            UpdateScoreSubmission(enabled); // laggy on the first time, idk why
            UpdateTexts();
        });

        // njs slider
        njsSlider = CreateIncrementSlider(mainVertical, "", currentAppliedValues.GetNJS(), 0.1, 1, 30, [](float value) {
            currentAppliedValues.SetNJS(value);
            UpdateTexts();
        });
        njsSlider = ReparentSlider(njsSlider, njsToggle);
        ((UnityEngine::RectTransform*) njsSlider->slider->get_transform())->set_anchoredPosition({-25, 0});

        SetActive(njsSlider, currentAppliedValues.GetOverrideNJS());
        SetActive(durationSlider, currentAppliedValues.GetUseDuration());
        SetActive(distanceSlider, !currentAppliedValues.GetUseDuration());

        auto presetsVertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject);
        presetsVertical->set_childControlHeight(false);
        presetsVertical->set_childForceExpandHeight(false);
        presetsVertical->set_childForceExpandWidth(true);
        presetsVertical->set_spacing(0.5);

        presetsParent = presetsVertical->get_gameObject();

        // raise up container
        presetsVertical->get_rectTransform()->set_anchoredPosition({0, 31});

        auto horizontal2 = BeatSaberUI::CreateHorizontalLayoutGroup(presetsVertical);
        horizontal2->set_spacing(1);

        CreateSmallButton(horizontal2, "Back", []() {
            mainParent->SetActive(true);
            presetsParent->SetActive(false);
        });

        auto presetLabel = BeatSaberUI::CreateText(horizontal2->get_transform(), "Presets");
        presetLabel->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(20);
        presetLabel->set_alignment(TMPro::TextAlignmentOptions::MidlineLeft);

        auto left = CreateSmallButton(horizontal2, "<", []() {});
        auto right = CreateSmallButton(horizontal2, ">", []() {});

        presetIncrement = BeatSaberUI::CreateIncrementSetting(gameObject, "", 0, 1, 1, 1, getModConfig().Presets.GetValue().size() + 1, [](float preset) {
            preset--; // goes 1, 2, 3... as displayed and the last is the main config
            if(preset == getModConfig().Presets.GetValue().size())
                currentModifiedValues = Preset(currentLevelValues);
            else
                currentModifiedValues = Preset(preset, currentLevelValues);
        });
        SetButtons(presetIncrement);
        auto incrementObject = presetIncrement->get_transform()->GetChild(1);
        incrementObject->SetParent(horizontal2->get_transform());
        incrementObject->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(30);

        CreateSmallButton(horizontal2, "-", []() {});
        CreateSmallButton(horizontal2, "+", []() {});

        auto horizontal3 = BeatSaberUI::CreateHorizontalLayoutGroup(presetsVertical);
        horizontal3->set_spacing(1);

        CreateNonSettingToggle(presetsVertical, getModConfig().AutoDef, currentModifiedValues.GetUseDefaults(), [](bool enabled) {
            currentModifiedValues.SetUseDefaults(enabled);
            currentAppliedValues.UpdateLevel(currentLevelValues);
            UpdateMainUI();
        });

        std::vector<std::string> mainValueOptions = {"Distance", "Duration"};
        CreateNonSettingDropdownEnum(presetsVertical, getModConfig().UseDuration, currentModifiedValues.GetUseDuration(), mainValueOptions, [](int option) {
            currentModifiedValues.SetUseDuration(option);
            SetActive(durationSlider, option);
            SetActive(distanceSlider, !option);
        });

        CreateNonSettingToggle(presetsVertical, getModConfig().BoundJD, currentModifiedValues.GetUseBounds(), [](bool enabled) {
            currentModifiedValues.SetUseBounds(enabled);
            boundsParent->SetActive(enabled);
            UpdateTexts();
        });

        boundsParent = UnityEngine::GameObject::New_ctor("JDFixerWrapperObj");
        boundsParent->AddComponent<UnityEngine::RectTransform*>()->set_sizeDelta({90, 8});
        boundsParent->get_transform()->SetParent(presetsVertical->get_transform(), false);
        auto horizontal4 = BeatSaberUI::CreateHorizontalLayoutGroup(boundsParent);
        horizontal4->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredHeight(8);
        horizontal4->set_childControlWidth(false);
        horizontal4->get_rectTransform()->set_anchoredPosition({4, 0});

        ReparentSlider(CreateIncrementSlider(presetsVertical, "Min", currentModifiedValues.GetBoundMin(), 0.1, 1, 30, [](float value) {
            currentModifiedValues.SetBoundMin(value);
            UpdateTexts();
        }, 34), horizontal4);
        CreateCenteredText(horizontal4, "to");
        ReparentSlider(CreateIncrementSlider(presetsVertical, "Max", currentModifiedValues.GetBoundMax(), 0.1, 1, 30, [](float value) {
            currentModifiedValues.SetBoundMax(value);
            UpdateTexts();
        }, 34), horizontal4);

        boundsParent->SetActive(currentModifiedValues.GetUseBounds());
    }
    mainParent->SetActive(true);
    presetsParent->SetActive(false);

    settingsGO = gameObject;
}
#pragma endregion

bool UpdatePreset() {
    std::string search = currentBeatmap->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(search);
    if(iter != map.end()) {
        // only reset when preset changes
        if(lastPreset.which != 0 || lastPreset.id != search) {
            currentAppliedValues = Preset(iter->second, currentLevelValues);
            lastPreset.id = search;
            lastPreset.which = 0;
            return true;
        }
        return false;
    }
    float nps = GetNPS(currentBeatmap);
    float bpm = GetBPM(currentBeatmap);
    auto presets = getModConfig().Presets.GetValue();
    for(int i = 0; i < presets.size(); i++) {
        if(ConditionMet(presets[i], currentLevelValues.njs, nps, bpm)) {
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

void UpdateLevel(GlobalNamespace::IDifficultyBeatmap* beatmap) {
    if(currentBeatmap == beatmap)
        return;
    // beatmap being non null here signifies that it changed
    if(beatmap) {
        currentBeatmap = beatmap;
        currentLevelValues = GetLevelDefaults(beatmap);
    }
    bool currentValuesChanged = UpdatePreset();
    if(currentValuesChanged)
        UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
    else if(beatmap)
        currentAppliedValues.UpdateLevel(currentLevelValues);
    else // if nothing was changed
        return;
    if(settingsGO)
        UpdateMainUI();
}

LevelPreset GetAppliedValues() {
    return currentAppliedValues.GetAsLevelPreset();
}
