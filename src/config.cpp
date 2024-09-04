#include <cstddef>
#include <iomanip>

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "HMUI/AnimatedSwitchView.hpp"
#include "HMUI/EventSystemListener.hpp"
#include "HMUI/HoverHintController.hpp"
#include "System/Action_1.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "UnityEngine/WaitForSeconds.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "custom-types/shared/delegate.hpp"
#include "main.hpp"
#include "utils.hpp"

DifficultyBeatmap currentBeatmap = {};
Values currentLevelValues = {};
float lastSpeed = 0;
float currentNps = 0;

Preset currentAppliedValues;
Indicator lastPreset;
bool hasLevelPreset;

Preset currentModifiedValues;

#pragma region ui
static constexpr int maxFittingConditions = 3;

BSML::SliderSetting *durationSlider, *distanceSlider, *njsSlider, *minBoundSlider, *maxBoundSlider;
BSML::ClickableText *distanceText, *durationText, *njsText;
BSML::ToggleSetting *njsToggle, *useDefaultsToggle, *useBoundsToggle, *levelSaveToggle;
UnityEngine::UI::Button *removeButton, *leftButton, *rightButton, *minusButton;
BSML::IncrementSetting* presetIncrement;
BSML::DropdownListSetting* durDistDropdown;
TMPro::TextMeshProUGUI* levelStatusText;

UnityW<UnityEngine::GameObject> settingsGO;
UnityEngine::GameObject *mainParent, *presetsParent, *conditionsParent, *boundsParent;

BSML::SliderSetting* ReparentSlider(BSML::SliderSetting* slider, BSML::Lite::TransformWrapper parent, float width = 52) {
    auto newSlider = slider->slider->gameObject->AddComponent<BSML::SliderSetting*>();
    newSlider->slider = slider->slider;
    newSlider->onChange = std::move(slider->onChange);
    newSlider->formatter = std::move(slider->formatter);
    newSlider->isInt = slider->isInt;
    newSlider->increments = slider->increments;
    newSlider->slider->minValue = slider->slider->minValue;
    newSlider->slider->maxValue = slider->slider->maxValue;
    auto transform = newSlider->GetComponent<UnityEngine::RectTransform*>();
    transform->sizeDelta = {width, 0};
    transform->SetParent(parent->transform, false);
    UnityEngine::Object::DestroyImmediate(slider->gameObject);
    // due to the weird way bsml does string formatting for sliders,
    // this needs to be called after destroying the old slider
    newSlider->Setup();
    return newSlider;
}

void SetSliderBounds(BSML::SliderSetting* slider, float min, float max, float increment) {
    float value = slider->get_Value();
    slider->increments = increment;
    slider->isInt = abs(increment - round(increment)) < 0.000001;
    slider->slider->minValue = min;
    slider->slider->maxValue = max;
    slider->slider->numberOfSteps = ((max - min) / increment) + 1;
    slider->slider->SetNormalizedValue(slider->slider->NormalizeValue(value));
    slider->text->text = slider->TextForValue(slider->get_Value());
}

#include "bsml/shared/BSML.hpp"

BSML::ClickableText*
CreateClickableTextPatch(BSML::Lite::TransformWrapper parent, std::string text, TMPro::FontStyles, std::function<void()> callback) {
    auto parser = BSML::parse_and_construct(fmt::format("<clickable-text tags=\"ret\" italics=\"false\" text=\"{}\" />", text), parent.transform);
    auto vec = parser->parserParams->GetObjectsWithTag("ret");
    if (vec.empty()) {
        logger.error("tag didn't work");
        return nullptr;
    }
    auto ret = vec[0]->GetComponent<BSML::ClickableText*>();
    ret->onClick += {callback};
    return ret;
}

template <class F = nullptr_t>
inline auto CreateCenteredText(BSML::Lite::TransformWrapper parent, std::string text, float width = 0, F callback = nullptr) {
    auto retText = [&]() {
        if constexpr (std::is_same_v<F, std::nullptr_t>)
            return BSML::Lite::CreateText(parent, text, TMPro::FontStyles::Normal);
        else
            return CreateClickableTextPatch(parent, text, TMPro::FontStyles::Normal, callback);
    }();
    retText->alignment = TMPro::TextAlignmentOptions::Center;
    retText->rectTransform->sizeDelta = {width, 8};
    return retText;
}

inline UnityEngine::UI::Button* CreateSmallButton(BSML::Lite::TransformWrapper parent, std::string text, auto callback, std::string hint = "") {
    UnityEngine::UI::Button* button = BSML::Lite::CreateUIButton(parent, text, callback);
    button->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = -1;
    if (!hint.empty())
        BSML::Lite::AddHoverHint(button, hint);
    return button;
}

inline BSML::DropdownListSetting* CreateDropdownEnum(
    BSML::Lite::TransformWrapper parent,
    std::string name,
    int currentValue,
    std::vector<std::string_view>& dropdownStrings,
    auto intCallback,
    float width = 40
) {
    BSML::DropdownListSetting* object =
        BSML::Lite::CreateDropdown(parent, name, dropdownStrings[currentValue], dropdownStrings, [intCallback, dropdownStrings](StringW value) {
            for (int i = 0; i < dropdownStrings.size(); i++) {
                if (value == dropdownStrings[i]) {
                    intCallback(i);
                    break;
                }
            }
        });
    object->gameObject->AddComponent<UnityEngine::UI::LayoutElement*>()->preferredHeight = 7;
    object->GetComponent<UnityEngine::RectTransform*>()->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, width);
    return object;
}

template <class CV>
inline BSML::DropdownListSetting* CreateNonSettingDropdownEnum(
    BSML::Lite::TransformWrapper parent,
    ConfigUtils::ConfigValue<CV>& configValue,
    int currentValue,
    std::vector<std::string_view>& dropdownStrings,
    auto intCallback
) {
    auto object = CreateDropdownEnum(parent, configValue.GetName(), currentValue, dropdownStrings, intCallback);
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object, configValue.GetHoverHint());
    return object;
}

inline void ReparentDropdown(BSML::DropdownListSetting* dropdown, BSML::Lite::TransformWrapper parent) {
    auto oldParent = dropdown->transform->parent;
    dropdown->transform->SetParent(parent->transform, false);
    UnityEngine::Object::Destroy(oldParent->gameObject);
}

template <class CV>
inline BSML::ToggleSetting*
CreateNonSettingToggle(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<CV>& configValue, bool currentValue, auto callback) {
    BSML::ToggleSetting* object = BSML::Lite::CreateToggle(parent, configValue.GetName(), currentValue, callback);
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object, configValue.GetHoverHint());
    object->GetComponent<UnityEngine::RectTransform*>()->sizeDelta = {18, 8};
    return object;
}

inline BSML::ToggleSetting* AddConfigValueToggle(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = CreateNonSettingToggle(parent, configValue, configValue.GetValue(), [&configValue, callback](bool value) {
        configValue.SetValue(value);
        callback(value);
    });
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object, configValue.GetHoverHint());
    return object;
}

inline void SetSliderName(BSML::SliderSetting* slider, std::string const& name) {
    auto text = slider->transform->Find("NameText");
    if (text)
        text->GetComponent<TMPro::TextMeshProUGUI*>()->text = name;
}

inline void InstantSetToggle(BSML::ToggleSetting* setting, bool value) {
    auto toggle = setting->toggle;
    if (toggle->m_IsOn == value)
        return;
    toggle->m_IsOn = value;
    auto animatedSwitch = toggle->GetComponent<HMUI::AnimatedSwitchView*>();
    animatedSwitch->HandleOnValueChanged(value);
    animatedSwitch->_switchAmount = value;
    animatedSwitch->LerpPosition(value);
    animatedSwitch->LerpColors(value, animatedSwitch->_highlightAmount, animatedSwitch->_disabledAmount);
}

inline void SetActive(auto component, bool active) {
    component->gameObject->active = active;
}

custom_types::Helpers::Coroutine HideHintsCoro() {
    co_yield (System::Collections::IEnumerator*) UnityEngine::WaitForSeconds::New_ctor(0.3);
    UnityEngine::Object::FindObjectOfType<HMUI::HoverHintController*>()->HideHintInstant();
    co_return;
}

inline std::string FormatDecimals(float value, int decimals) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
}

inline void SetText(TMPro::TextMeshProUGUI* text, float main, float secondary) {
    std::string mainStr = FormatDecimals(main, getModConfig().Decimals.GetValue() - 1);
    std::string secondaryStr = FormatDecimals(secondary, getModConfig().Decimals.GetValue() - 1);
    if (mainStr == secondaryStr || secondary == 0)
        text->text = fmt::format("<#5ec462>{}", secondaryStr);
    else
        text->text = fmt::format("<#ff6969>{} <size=75%><#5ec462>({})", mainStr, secondaryStr);
}

inline void ReplaceAll(std::string& text, std::string_view repl, std::string_view fill) {
    auto pos = text.find(repl);
    while (pos != std::string::npos) {
        text.replace(pos, repl.size(), fill);
        pos = text.find(repl);
    }
}

void Highlight(TMPro::TextMeshProUGUI* text) {
    std::string str = text->text;
    ReplaceAll(str, "#5ec462", "#a9c7aa");
    ReplaceAll(str, "#ff6969", "#ffb3b3");
    text->text = str;
}

void Unhighlight(TMPro::TextMeshProUGUI* text) {
    std::string str = text->text;
    ReplaceAll(str, "#a9c7aa", "#5ec462");
    ReplaceAll(str, "#ffb3b3", "#ff6969");
    text->text = str;
}

void UpdateTexts() {
    SetText(durationText, currentAppliedValues.GetDuration(), currentLevelValues.GetJumpDuration());
    SetText(distanceText, currentAppliedValues.GetDistance(), currentLevelValues.GetJumpDistance());
    SetText(njsText, currentAppliedValues.GetNJS(), currentLevelValues.njs);
}

void UpdateLevelSaveStatus() {
    InstantSetToggle(levelSaveToggle, currentAppliedValues.GetIsLevelPreset() && currentAppliedValues.GetAsLevelPreset().Active);
    levelStatusText->text = hasLevelPreset ? "Settings Exist" : "No Settings Exist";
    removeButton->interactable = hasLevelPreset;
}

void UpdateDecimals() {
    UpdateTexts();
    float newIncrements = 1.0 / pow(10, getModConfig().Decimals.GetValue());
    SetSliderBounds(durationSlider, durationSlider->slider->minValue, durationSlider->slider->maxValue, newIncrements);
    SetSliderBounds(distanceSlider, distanceSlider->slider->minValue, distanceSlider->slider->maxValue, newIncrements);
    SetSliderBounds(njsSlider, njsSlider->slider->minValue, njsSlider->slider->maxValue, newIncrements);
}

void UpdateMainUI() {
    UpdateTexts();
    SetActive(durationSlider, currentAppliedValues.GetUseDuration());
    SetActive(distanceSlider, !currentAppliedValues.GetUseDuration());
    SetSliderName(durationSlider, getModConfig().Half.GetValue() ? "Half Jump Duration" : "Jump Duration");
    SetSliderName(distanceSlider, getModConfig().Half.GetValue() ? "Half Jump Distance" : "Jump Distance");
    if (currentAppliedValues.GetUseDuration())
        durationSlider->set_Value(currentAppliedValues.GetMainValue());
    else
        distanceSlider->set_Value(currentAppliedValues.GetMainValue());
    InstantSetToggle(njsToggle, currentAppliedValues.GetOverrideNJS());
    SetActive(njsSlider, currentAppliedValues.GetOverrideNJS());
    njsSlider->set_Value(currentAppliedValues.GetNJS());
    UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
    UpdateLevelSaveStatus();
}

void UpdateConditions() {
    auto transform = conditionsParent->transform;
    int children = transform->GetChildCount();
    for (int i = 0; i < children; i++) {
        if (i < currentModifiedValues.GetConditionCount() * 4) {
            auto cond = currentModifiedValues.GetCondition(i / 4);
            auto child = transform->GetChild(i);
            switch (i % 4) {
                case 0:
                    if (i != 0)
                        child->GetComponent<HMUI::SimpleTextDropdown*>()->SelectCellWithIdx(cond.AndOr);
                    break;
                case 1:
                    child->GetComponent<HMUI::SimpleTextDropdown*>()->SelectCellWithIdx(cond.Type);
                    break;
                case 2:
                    child->GetComponent<HMUI::SimpleTextDropdown*>()->SelectCellWithIdx(cond.Comparison);
                    break;
                case 3: {
                    auto slider = child->GetChild(0)->GetComponent<BSML::SliderSetting*>();
                    slider->set_Value(cond.Value);
                    if (cond.Type == 2) {
                        slider->formatter = nullptr;
                        SetSliderBounds(slider, 0, 1000, 10);
                    } else {
                        slider->formatter = [](float value) {
                            return FormatDecimals(value, 1);
                        };
                        SetSliderBounds(slider, 0, 30, 0.1);
                    }
                    break;
                }
            }
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
    std::string str = std::to_string((int) presetIncrement->currentValue);
    if (presetIncrement->currentValue == getModConfig().Presets.GetValue().size() + 1)
        str = "Default";
    presetIncrement->text->text = str;
    auto buttons = presetIncrement->text->transform->parent->GetComponentsInChildren<UnityEngine::UI::Button*>();
    buttons->First()->interactable = presetIncrement->currentValue > presetIncrement->minValue;
    buttons->Last()->interactable = presetIncrement->currentValue < presetIncrement->maxValue;
}

void UpdatePresetUI() {
    UpdateConditions();
    UpdatePresetControl();
    InstantSetToggle(useDefaultsToggle, currentModifiedValues.GetUseDefaults());
    durDistDropdown->dropdown->SelectCellWithIdx(currentModifiedValues.GetUseDuration());
    InstantSetToggle(useBoundsToggle, currentModifiedValues.GetUseBounds());
    boundsParent->SetActive(currentModifiedValues.GetUseBounds());
    minBoundSlider->set_Value(currentModifiedValues.GetBoundMin());
    maxBoundSlider->set_Value(currentModifiedValues.GetBoundMax());
}

bool UpdatePreset();

void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation) {
    if (firstActivation) {

        auto mainVertical = BSML::Lite::CreateVerticalLayoutGroup(gameObject);
        mainVertical->childControlHeight = false;
        mainVertical->childForceExpandHeight = false;
        mainVertical->childForceExpandWidth = true;
        mainVertical->spacing = 0.5;

        mainParent = mainVertical->gameObject;

        // raise up container
        mainVertical->rectTransform->anchoredPosition = {0, 4};

        auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(mainVertical);
        horizontal->spacing = 5;

        auto titleText = BSML::Lite::CreateText(horizontal, "Main Settings");
        titleText->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 15;
        titleText->alignment = TMPro::TextAlignmentOptions::MidlineLeft;

        CreateSmallButton(
            horizontal,
            "Presets",
            []() {
                mainParent->active = false;
                presetsParent->active = true;
                currentModifiedValues.SyncCondition();
            },
            "Modify conditional presets"
        );

        auto enableToggle =
            AddConfigValueToggle(horizontal, getModConfig().Disable, [](bool) { UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS()); });
        enableToggle->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 30;

        auto spaced = BSML::Lite::CreateGridLayoutGroup(mainVertical);
        spaced->constraint = UnityEngine::UI::GridLayoutGroup::Constraint::FixedRowCount;
        spaced->constraintCount = 2;
        spaced->cellSize = {30, 8};
        spaced->rectTransform->sizeDelta = {0, 12};
        spaced->spacing = {0, -2};

        UnityEngine::Color labelColor(0.3, 0.7, 1, 1);
        CreateCenteredText(spaced, "Duration")->color = labelColor;
        CreateCenteredText(spaced, "Distance")->color = labelColor;
        CreateCenteredText(spaced, "NJS")->color = labelColor;

        durationText = CreateCenteredText(spaced, "", 0, []() {
            if (!currentBeatmap)
                return;
            currentAppliedValues.SetDuration(currentLevelValues.GetJumpDuration());
            UpdateMainUI();
        });
        SetText(durationText, 0, 0);
        durationText->onEnter = std::bind(Highlight, durationText);
        durationText->onExit = std::bind(Unhighlight, durationText);

        distanceText = CreateCenteredText(spaced, "", 0, []() {
            if (!currentBeatmap)
                return;
            currentAppliedValues.SetDistance(currentLevelValues.GetJumpDistance());
            UpdateMainUI();
        });
        SetText(distanceText, 0, 0);
        distanceText->onEnter = std::bind(Highlight, distanceText);
        distanceText->onExit = std::bind(Unhighlight, distanceText);

        njsText = CreateCenteredText(spaced, "", 0, []() {
            if (!currentBeatmap)
                return;
            currentAppliedValues.SetNJS(currentLevelValues.njs);
            UpdateMainUI();
        });
        SetText(njsText, 0, 0);
        njsText->onEnter = std::bind(Highlight, njsText);
        njsText->onExit = std::bind(Unhighlight, njsText);

        for (int i = 0; i < spaced->transform->GetChildCount(); i++)
            BSML::Lite::AddHoverHint(
                spaced->transform->GetChild(i), "The default for the level (green) and the applied (red) values. Click to set to the level default"
            );

        auto decimalFormat = [](float value) {
            return FormatDecimals(value, getModConfig().Decimals.GetValue());
        };
        float decimalIncrement = 1.0 / pow(10, getModConfig().Decimals.GetValue());

        durationSlider = BSML::Lite::CreateSliderSetting(
            mainVertical,
            "Half Jump Duration",
            decimalIncrement,
            currentAppliedValues.GetMainValue(),
            0.1,
            1.5,
            0,
            true,
            {},
            [](float value) {
                if (!currentAppliedValues.GetUseDuration())
                    return;
                currentAppliedValues.SetMainValue(value);
                UpdateTexts();
            }
        );
        durationSlider->formatter = decimalFormat;
        distanceSlider = BSML::Lite::CreateSliderSetting(
            mainVertical,
            "Half Jump Distance",
            decimalIncrement,
            currentAppliedValues.GetMainValue(),
            1,
            30,
            0,
            true,
            {},
            [](float value) {
                if (currentAppliedValues.GetUseDuration())
                    return;
                currentAppliedValues.SetMainValue(value);
                UpdateTexts();
            }
        );
        distanceSlider->formatter = decimalFormat;

        // njs toggle
        njsToggle = CreateNonSettingToggle(mainVertical, getModConfig().UseNJS, currentAppliedValues.GetOverrideNJS(), [](bool enabled) {
            if (enabled == currentAppliedValues.GetOverrideNJS())
                return;
            SetActive(njsSlider, enabled);
            currentAppliedValues.SetOverrideNJS(enabled);
            UpdateScoreSubmission(enabled);
            UpdateTexts();
        });

        // njs slider
        njsSlider =
            BSML::Lite::CreateSliderSetting(mainVertical, "", decimalIncrement, currentAppliedValues.GetNJS(), 1, 30, 0, true, {}, [](float value) {
                currentAppliedValues.SetNJS(value);
                UpdateTexts();
            });
        njsSlider = ReparentSlider(njsSlider, njsToggle);
        njsSlider->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {-20, 0};
        njsSlider->formatter = decimalFormat;

        auto listener = njsSlider->gameObject->AddComponent<HMUI::EventSystemListener*>();
        listener->add_pointerDidEnterEvent(custom_types::MakeDelegate<System::Action_1<UnityEngine::EventSystems::PointerEventData*>*>(
            (std::function<void(UnityEngine::EventSystems::PointerEventData*)>) [](UnityEngine::EventSystems::PointerEventData*) {
                njsSlider->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(HideHintsCoro()));
            }
        ));

        auto horizontal1 = BSML::Lite::CreateHorizontalLayoutGroup(mainVertical);
        horizontal1->childAlignment = UnityEngine::TextAnchor::MiddleCenter;
        horizontal1->childForceExpandWidth = false;
        horizontal1->spacing = 1;

        bool presetActive = currentAppliedValues.GetIsLevelPreset() && currentAppliedValues.GetAsLevelPreset().Active;
        levelSaveToggle = BSML::Lite::CreateToggle(horizontal1, "Level Specific Settings", presetActive, [](bool enabled) {
            if (!currentBeatmap)
                return;
            auto currentLevelPreset = currentAppliedValues.GetAsLevelPreset();
            if (auto preexisting = GetLevelPreset(currentBeatmap))
                currentLevelPreset = preexisting->second;
            currentLevelPreset.Active = enabled;
            SetLevelPreset(currentBeatmap, currentLevelPreset);
            if (UpdatePreset())
                UpdateMainUI();
        });
        BSML::Lite::AddHoverHint(levelSaveToggle, "Use and change settings specifically for the currently selected level instead of a preset");
        levelSaveToggle->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 50;

        levelStatusText = CreateCenteredText(horizontal1, hasLevelPreset ? "Settings Exist" : "No Settings Exist");
        levelStatusText->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(30);
        removeButton = CreateSmallButton(
            horizontal1,
            "X",
            []() {
                if (!currentBeatmap)
                    return;
                RemoveLevelPreset(currentBeatmap);
                if (UpdatePreset())
                    UpdateMainUI();
                else
                    UpdateLevelSaveStatus();
            },
            "Remove specific settings for this level"
        );
        removeButton->interactable = hasLevelPreset;
        removeButton->GetComponent<UnityEngine::RectTransform*>()->sizeDelta = {8, 8};

        auto horizontal2 = BSML::Lite::CreateHorizontalLayoutGroup(mainVertical);
        horizontal2->spacing = 6;

        auto practiceToggle = AddConfigValueToggle(horizontal2, getModConfig().Practice);
        practiceToggle->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 42;
        auto halfToggle = AddConfigValueToggle(horizontal2, getModConfig().Half, [](bool) { UpdateMainUI(); });
        halfToggle->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 42;

        auto decimalIncrementSetting = AddConfigValueIncrementInt(mainVertical, getModConfig().Decimals, 1, 1, 3);
        decimalIncrementSetting->onChange = [oldCallback = std::move(decimalIncrementSetting->onChange)](float value) {
            oldCallback(value);
            UpdateDecimals();
        };

        auto presetsVertical = BSML::Lite::CreateVerticalLayoutGroup(gameObject);
        presetsVertical->childControlHeight = false;
        presetsVertical->childForceExpandHeight = false;
        presetsVertical->childForceExpandWidth = true;

        presetsParent = presetsVertical->gameObject;

        // raise up container
        presetsVertical->rectTransform->anchoredPosition = {0, 4};

        auto horizontal3 = BSML::Lite::CreateHorizontalLayoutGroup(presetsVertical);
        horizontal3->spacing = 1;

        CreateSmallButton(horizontal3, "Back", []() {
            mainParent->active = true;
            presetsParent->active = false;
        });

        auto presetLabel = BSML::Lite::CreateText(horizontal3, "Presets");
        presetLabel->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 20;
        presetLabel->alignment = TMPro::TextAlignmentOptions::MidlineLeft;

        leftButton = CreateSmallButton(
            horizontal3,
            "<",
            []() {
                if (currentModifiedValues.ShiftBackward()) {
                    if (lastPreset.which == 1 && lastPreset.idx != -1)
                        lastPreset.idx--;
                    currentAppliedValues.SyncCondition(-1);
                    presetIncrement->currentValue--;
                    UpdatePresetControl();
                }
            },
            "Move preset up in priority"
        );
        rightButton = CreateSmallButton(
            horizontal3,
            ">",
            []() {
                if (currentModifiedValues.ShiftForward()) {
                    if (lastPreset.which == 1 && lastPreset.idx != -1)
                        lastPreset.idx++;
                    currentAppliedValues.SyncCondition(1);
                    presetIncrement->currentValue++;
                    UpdatePresetControl();
                }
            },
            "Move preset down in priority"
        );

        int presetNum = getModConfig().Presets.GetValue().size();
        presetIncrement = BSML::Lite::CreateIncrementSetting(gameObject, "", 0, 1, presetNum + 1, 1, presetNum + 1, [](float preset) {
            preset--;  // goes 1, 2, 3... as displayed and the last is the main config
            if (preset == getModConfig().Presets.GetValue().size())
                currentModifiedValues = Preset(currentLevelValues, false);
            else
                currentModifiedValues = Preset(preset, currentLevelValues, false);
            UpdatePresetUI();
        });
        SetButtons(presetIncrement);
        auto incrementObject = presetIncrement->transform->GetChild(1);
        incrementObject->SetParent(horizontal3->transform);
        incrementObject->gameObject->AddComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 30;

        minusButton = CreateSmallButton(
            horizontal3,
            "-",
            []() {
                int idx = currentModifiedValues.GetConditionPresetIndex();
                if (idx == -1)
                    return;
                auto presets = getModConfig().Presets.GetValue();
                presets.erase(presets.begin() + idx);
                getModConfig().Presets.SetValue(presets);
                presetIncrement->maxValue = presets.size() + 1;
                presetIncrement->EitherPressed();
                if (UpdatePreset())
                    UpdateMainUI();
            },
            "Delete current preset"
        );
        CreateSmallButton(
            horizontal3,
            "+",
            []() {
                auto presets = getModConfig().Presets.GetValue();
                presets.emplace_back();
                getModConfig().Presets.SetValue(presets);
                presetIncrement->currentValue = presets.size();
                presetIncrement->maxValue = presets.size() + 1;
                presetIncrement->EitherPressed();
                if (UpdatePreset())
                    UpdateMainUI();
            },
            "Add new preset"
        );

        auto spaced2 = BSML::Lite::CreateGridLayoutGroup(presetsVertical);
        spaced2->constraint = UnityEngine::UI::GridLayoutGroup::Constraint::FixedColumnCount;
        spaced2->constraintCount = 4;
        spaced2->cellSize = {22.5, 8};
        spaced2->spacing = {0, 0.5};
        spaced2->GetComponent<UnityEngine::UI::ContentSizeFitter*>()->verticalFit = UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize;
        conditionsParent = spaced2->gameObject;

        static std::vector<std::string_view> options1 = {"And", "Or", "Delete"};
        static std::vector<std::string_view> options2 = {"NPS", "NJS", "BPM"};
        static std::vector<std::string_view> options3 = {"Under", "Over"};
        for (int i = 0; i < maxFittingConditions; i++) {
            if (i == 0) {
                auto horizontal4 = BSML::Lite::CreateHorizontalLayoutGroup(spaced2);
                horizontal4->childControlWidth = true;
                CreateSmallButton(
                    horizontal4,
                    "+",
                    []() {
                        if (currentModifiedValues.GetConditionCount() < maxFittingConditions)
                            currentModifiedValues.SetCondition({}, maxFittingConditions);
                        UpdateConditions();
                    },
                    "Add new condition for the current preset"
                );
                CreateCenteredText(horizontal4, "On", 12);
            } else {
                ReparentDropdown(
                    CreateDropdownEnum(
                        presetsVertical,
                        "",
                        currentModifiedValues.GetCondition(i).AndOr,
                        options1,
                        [i](int option) {
                            if (option != 2) {
                                auto cond = currentModifiedValues.GetCondition(i);
                                cond.AndOr = option;
                                currentModifiedValues.SetCondition(cond, i);
                                if (UpdatePreset())
                                    UpdateMainUI();
                                else
                                    currentAppliedValues.SyncCondition();
                            } else {
                                currentModifiedValues.RemoveCondition(i);
                                UpdateConditions();
                                if (UpdatePreset())
                                    UpdateMainUI();
                            }
                        },
                        22
                    ),
                    spaced2
                );
            }
            ReparentDropdown(
                CreateDropdownEnum(
                    presetsVertical,
                    "",
                    currentModifiedValues.GetCondition(i).Type,
                    options2,
                    [i](int option) {
                        auto cond = currentModifiedValues.GetCondition(i);
                        cond.Type = option;
                        currentModifiedValues.SetCondition(cond, i);
                        currentAppliedValues.SyncCondition();
                        UpdateConditions();
                        if (UpdatePreset())
                            UpdateMainUI();
                    },
                    22
                ),
                spaced2
            );
            ReparentDropdown(
                CreateDropdownEnum(
                    presetsVertical,
                    "",
                    currentModifiedValues.GetCondition(i).Comparison,
                    options3,
                    [i](int option) {
                        auto cond = currentModifiedValues.GetCondition(i);
                        cond.Comparison = option;
                        currentModifiedValues.SetCondition(cond, i);
                        currentAppliedValues.SyncCondition();
                        if (UpdatePreset())
                            UpdateMainUI();
                    },
                    22
                ),
                spaced2
            );
            auto slider =
                BSML::Lite::CreateSliderSetting(presetsVertical, "", 0.1, currentModifiedValues.GetCondition(i).Value, 0, 1000, 0, [i](float value) {
                    auto cond = currentModifiedValues.GetCondition(i);
                    cond.Value = value;
                    currentModifiedValues.SetCondition(cond, i);
                    currentAppliedValues.SyncCondition();
                    if (UpdatePreset())
                        UpdateMainUI();
                });
            slider->slider->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {9.5, 0};
            auto wrapper = UnityEngine::GameObject::New_ctor("JDFixerSliderWrapper");
            // wrapper->transform->SetParent(spaced2->transform, false);
            wrapper->AddComponent<UnityEngine::RectTransform*>()->SetParent(spaced2->transform, false);
            ReparentSlider(slider, wrapper, 32);
        }

        useDefaultsToggle = CreateNonSettingToggle(presetsVertical, getModConfig().AutoDef, currentModifiedValues.GetUseDefaults(), [](bool enabled) {
            if (enabled == currentModifiedValues.GetUseDefaults())
                return;
            currentModifiedValues.SetUseDefaults(enabled);
            currentAppliedValues.SyncCondition();
            if (!currentBeatmap)
                return;
            currentAppliedValues.UpdateLevel(currentLevelValues);
            UpdateMainUI();
        });

        static std::vector<std::string_view> mainValueOptions = {"Distance", "Duration"};
        durDistDropdown = CreateNonSettingDropdownEnum(
            presetsVertical,
            getModConfig().UseDuration,
            currentModifiedValues.GetUseDuration(),
            mainValueOptions,
            [](int option) {
                currentModifiedValues.SetUseDuration(option);
                currentAppliedValues.SyncCondition();
                UpdateMainUI();
            }
        );

        useBoundsToggle = CreateNonSettingToggle(presetsVertical, getModConfig().BoundJD, currentModifiedValues.GetUseBounds(), [](bool enabled) {
            if (enabled == currentModifiedValues.GetUseBounds())
                return;
            currentModifiedValues.SetUseBounds(enabled);
            currentAppliedValues.SyncCondition();
            boundsParent->SetActive(enabled);
            UpdateTexts();
        });

        boundsParent = UnityEngine::GameObject::New_ctor("JDFixerBoundsParent");
        boundsParent->AddComponent<UnityEngine::RectTransform*>()->sizeDelta = {90, 8};
        boundsParent->transform->SetParent(presetsVertical->transform, false);
        auto horizontal5 = BSML::Lite::CreateHorizontalLayoutGroup(boundsParent);
        horizontal5->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredHeight = 8;
        horizontal5->childControlWidth = false;

        minBoundSlider = ReparentSlider(
            BSML::Lite::CreateSliderSetting(
                presetsVertical,
                "Min",
                0.1,
                currentModifiedValues.GetBoundMin(),
                0,
                30,
                0,
                true,
                {},
                [](float value) {
                    currentModifiedValues.SetBoundMin(value);
                    currentAppliedValues.SyncCondition();
                    UpdateTexts();
                }
            ),
            horizontal5,
            48
        );
        minBoundSlider->formatter = [](float value) {
            return FormatDecimals(value, 1);
        };
        CreateCenteredText(horizontal5, "to", 3);
        maxBoundSlider = ReparentSlider(
            BSML::Lite::CreateSliderSetting(
                presetsVertical,
                "Max",
                0.1,
                currentModifiedValues.GetBoundMax(),
                1,
                30,
                0,
                true,
                {},
                [](float value) {
                    currentModifiedValues.SetBoundMax(value);
                    currentAppliedValues.SyncCondition();
                    UpdateTexts();
                }
            ),
            horizontal5,
            48
        );
        maxBoundSlider->formatter = [](float value) {
            return FormatDecimals(value, 1);
        };

        UpdateMainUI();
        UpdatePresetUI();
    }
    mainParent->SetActive(true);
    presetsParent->SetActive(false);

    settingsGO = gameObject;
}
#pragma endregion

bool UpdatePreset() {
    if (!currentBeatmap)
        return false;
    if (auto levelPreset = GetLevelPreset(currentBeatmap)) {
        hasLevelPreset = true;
        auto& [indicator, preset] = *levelPreset;
        if (preset.Active) {
            // only reset when preset changes
            if (lastPreset.which != 0 || lastPreset.id != indicator.id || lastPreset.map != indicator.map) {
                currentAppliedValues = Preset(indicator.id, indicator.map, currentLevelValues);
                lastPreset.id = indicator.id;
                lastPreset.map = indicator.map;
                lastPreset.which = 0;
                return true;
            }
            return false;
        }
    } else
        hasLevelPreset = false;
    float bpm = GetBPM(currentBeatmap.level);
    auto presets = getModConfig().Presets.GetValue();
    for (int i = 0; i < presets.size(); i++) {
        if (ConditionMet(presets[i], currentLevelValues.njs, currentNps, bpm)) {
            if (lastPreset.which != 1 || lastPreset.idx != i) {
                currentAppliedValues = Preset(i, currentLevelValues);
                lastPreset.idx = i;
                lastPreset.which = 1;
                return true;
            }
            return false;
        }
    }
    if (lastPreset.which != 1 || lastPreset.idx != -1) {
        currentAppliedValues = Preset(currentLevelValues);
        lastPreset.idx = -1;
        lastPreset.which = 1;
        return true;
    }
    return false;
}

void UpdateLevel(DifficultyBeatmap beatmap, float speed) {
    if (beatmap)
        logger.info(
            "Updating for level {} {} {}",
            beatmap.difficulty.levelId,
            (int) beatmap.difficulty.difficulty,
            beatmap.difficulty.beatmapCharacteristic->serializedName
        );
    if (!currentBeatmap)
        lastSpeed = speed;
    if (currentBeatmap == beatmap)
        return;
    // beatmap being non null here signifies that it changed
    if (beatmap)
        currentBeatmap = beatmap;
    currentLevelValues = GetLevelDefaults(currentBeatmap, speed);
    bool currentValuesChanged = UpdatePreset();
    if (currentValuesChanged)
        UpdateScoreSubmission(currentAppliedValues.GetOverrideNJS());
    else if (beatmap || lastSpeed != speed)
        currentAppliedValues.UpdateLevel(currentLevelValues);
    else  // if nothing was changed
        return;
    if (settingsGO)
        UpdateMainUI();
    lastSpeed = speed;
}

void UpdateNotesPerSecond(float nps) {
    currentNps = nps;
    UpdateLevel({}, lastSpeed);
}

LevelPreset GetAppliedValues() {
    return currentAppliedValues.GetAsLevelPreset();
}
