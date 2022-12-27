#include "main.hpp"
#include "utils.hpp"
#include "ModConfig.hpp"

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

using namespace QuestUI;
using namespace HMUI;

QuestUI::SliderSetting *distanceSlider, *durationSlider, *minJDSlider, *maxJDSlider, *njsSlider;

SafePtrUnity<UnityEngine::GameObject> settingsGO;

UnityEngine::Transform* GetSubcontainer(UnityEngine::UI::VerticalLayoutGroup* vertical) {
    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(vertical);
    horizontal->GetComponent<UnityEngine::UI::LayoutElement*>()->set_minHeight(8);
    horizontal->set_childForceExpandHeight(true);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    return horizontal->get_transform();
}

template<class T>
QuestUI::SliderSetting* CreateIncrementSlider(T* parent, ConfigUtils::ConfigValue<float>& configValue, float increment, float min, float max) {
    QuestUI::SliderSetting* slider = BeatSaberUI::CreateSliderSetting(parent, configValue.GetName(), 0.01, configValue.GetValue(), min, max, 0, [&configValue](float value) {
        configValue.SetValue(value);
    });
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(slider->slider, configValue.GetHoverHint());

    ((UnityEngine::RectTransform*) slider->slider->get_transform())->set_anchoredPosition({-6, 0});

    auto leftButton = BeatSaberUI::CreateUIButton(slider->get_transform(), "", "DecButton", {-3.5, 0}, {6, 8}, [slider, increment](){
        float newValue = slider->slider->get_value() - increment;
        slider->slider->set_value(newValue);
        // get_value to let it handle min/max
        slider->OnChange(slider->slider, slider->slider->get_value());
    });
    auto rightButton = QuestUI::BeatSaberUI::CreateUIButton(slider->get_transform(), "", "IncButton", {41, 0}, {8, 8}, [slider, increment](){
        float newValue = slider->slider->get_value() + increment;
        slider->slider->set_value(newValue);
        // get_value to let it handle min/max
        slider->OnChange(slider->slider, slider->slider->get_value());
    });

    return slider;
}

inline void AddConfigValueToggle(::UnityEngine::Transform* parent, ConfigUtils::ConfigValue<bool>& configValue, std::function<void(bool value)> callback) {
    auto object = BeatSaberUI::CreateToggle(parent, configValue.GetName(), configValue.GetValue(),
        [&configValue, callback = std::move(callback)](bool value) {
            configValue.SetValue(value);
            callback(value);
        }
    );
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(object->get_gameObject(), configValue.GetHoverHint());
}

template<class T>
inline void SetParentActive(T* component, bool active) {
    component->get_transform()->GetParent()->get_gameObject()->SetActive(active);
}

void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation) {
    if(firstActivation) {
        auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject->get_transform());

        // raise up container
        vertical->get_rectTransform()->set_anchoredPosition({0, 31});

        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().Disable, [](bool _) {
            UpdateScoreSubmission();
        });

        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().AutoDef, [](bool enabled) {
            if(enabled)
                SetToLevelDefaults();
        });

        // toggle to use distance or duration (off is distance, on is duration)
        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().AutoReact, [](bool enabled) {
            SetParentActive(distanceSlider, !enabled);
            SetParentActive(durationSlider, enabled);
        });

        // distance slider
        distanceSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().JumpDist, 0.1, 1, 25);

        // duration slider
        durationSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().ReactTime, 0.05, 0.1, 2.5);

        SetParentActive(distanceSlider, !getModConfig().AutoReact.GetValue());
        SetParentActive(durationSlider, getModConfig().AutoReact.GetValue());

        // toggle to bound jump distance
        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().BoundJD, [](bool enabled) {
            SetParentActive(minJDSlider, enabled);
            SetParentActive(maxJDSlider, enabled);
        });

        // bounds sliders
        minJDSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().MinJD, 0.1, 1, 25);
        maxJDSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().MaxJD, 0.1, 1, 25);

        SetParentActive(minJDSlider, getModConfig().BoundJD.GetValue());
        SetParentActive(maxJDSlider, getModConfig().BoundJD.GetValue());

        // njs toggle
        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().UseNJS, [](bool enabled) {
            SetParentActive(njsSlider, enabled);
            UpdateScoreSubmission();
        });

        // njs slider
        njsSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().NJS, 0.1, 1, 25);

        SetParentActive(njsSlider, getModConfig().UseNJS.GetValue());
    }
    settingsGO = gameObject;
}

void UpdateUI() {
    if(settingsGO.isAlive()) {
        distanceSlider->set_value(getModConfig().JumpDist.GetValue());
        durationSlider->set_value(getModConfig().ReactTime.GetValue());
        minJDSlider->set_value(getModConfig().MinJD.GetValue());
        maxJDSlider->set_value(getModConfig().MaxJD.GetValue());
        njsSlider->set_value(getModConfig().NJS.GetValue());
    }
}