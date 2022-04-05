#include "main.hpp"
#include "ModConfig.hpp"

#include "bs-utils/shared/utils.hpp"

#include "questui/shared/CustomTypes/Components/WeakPtrGO.hpp"

using namespace QuestUI;
using namespace HMUI;

QuestUI::SliderSetting *distanceSlider, *durationSlider, *minJDSlider, *maxJDSlider, *njsSlider;

WeakPtrGO<UnityEngine::GameObject> settingsGO;

UnityEngine::Transform* GetSubcontainer(UnityEngine::UI::VerticalLayoutGroup* vertical) {
    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(vertical);
    horizontal->GetComponent<UnityEngine::UI::LayoutElement*>()->set_minHeight(8);
    horizontal->set_childForceExpandHeight(true);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    return horizontal->get_transform();
}

template<class T>
QuestUI::SliderSetting* CreateIncrementSlider(T* parent, ConfigUtils::ConfigValue<float>& configValue, float increment, float min, float max) {
    QuestUI::SliderSetting* slider = BeatSaberUI::CreateSliderSetting(parent, configValue.GetName(), increment, configValue.GetValue(), min, max, [&configValue](float value) {
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

template<class T>
inline void SetParentActive(T* component, bool active) {
    component->get_transform()->GetParent()->get_gameObject()->SetActive(active);
}

void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation) {
    if(firstActivation) {
        settingsGO = gameObject;

        auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject->get_transform());
        
        // raise up container
        vertical->get_rectTransform()->set_anchoredPosition({0, 31});

        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().Disable);
        AddConfigValueToggle(GetSubcontainer(vertical), getModConfig().AutoDef);

        // toggle to use distance or duration (off is distance, on is duration)
        auto distanceDurationToggle = BeatSaberUI::CreateToggle(GetSubcontainer(vertical), getModConfig().AutoReact.GetName(), getModConfig().AutoReact.GetValue(), [](bool enabled) {
            getModConfig().AutoReact.SetValue(enabled);
            SetParentActive(distanceSlider, !enabled);
            SetParentActive(durationSlider, enabled);
        });
        BeatSaberUI::AddHoverHint(distanceDurationToggle, getModConfig().AutoReact.GetName());
        
        // distance slider
        distanceSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().JumpDist, 0.1, 1, 25);

        // duration slider
        durationSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().ReactTime, 0.05, 0.1, 5);
        
        SetParentActive(distanceSlider, !getModConfig().AutoReact.GetValue());
        SetParentActive(durationSlider, getModConfig().AutoReact.GetValue());

        // toggle to bound jump distance
        auto boundsToggle = BeatSaberUI::CreateToggle(GetSubcontainer(vertical), getModConfig().BoundJD.GetName(), getModConfig().BoundJD.GetValue(), [](bool enabled) {
            getModConfig().BoundJD.SetValue(enabled);
            SetParentActive(minJDSlider, enabled);
            SetParentActive(maxJDSlider, enabled);
        });
        BeatSaberUI::AddHoverHint(boundsToggle, getModConfig().BoundJD.GetHoverHint());

        // bounds sliders
        minJDSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().MinJD, 0.1, 1, 25);
        maxJDSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().MaxJD, 0.1, 1, 25);
        
        SetParentActive(minJDSlider, getModConfig().BoundJD.GetValue());
        SetParentActive(maxJDSlider, getModConfig().BoundJD.GetValue());

        // njs toggle
        auto njsToggle = BeatSaberUI::CreateToggle(GetSubcontainer(vertical), getModConfig().UseNJS.GetName(), getModConfig().UseNJS.GetValue(), [](bool enabled) {
            getModConfig().UseNJS.SetValue(enabled);
            SetParentActive(njsSlider, enabled);
            if(enabled)
                bs_utils::Submission::disable(getModInfo());
            else
                bs_utils::Submission::enable(getModInfo());
        });
        BeatSaberUI::AddHoverHint(njsToggle, getModConfig().UseNJS.GetHoverHint());

        // njs slider
        njsSlider = CreateIncrementSlider(GetSubcontainer(vertical), getModConfig().NJS, 0.1, 1, 25);

        SetParentActive(njsSlider, getModConfig().UseNJS.GetValue());
    }
}

void UpdateUI() {
    if(settingsGO.isValid()) {
        distanceSlider->set_value(getModConfig().JumpDist.GetValue());
        durationSlider->set_value(getModConfig().ReactTime.GetValue());
        minJDSlider->set_value(getModConfig().MinJD.GetValue());
        maxJDSlider->set_value(getModConfig().MaxJD.GetValue());
        njsSlider->set_value(getModConfig().NJS.GetValue());
    }
}