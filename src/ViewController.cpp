#include "main.hpp"
#include "ModConfig.hpp"

#include "beatsaber-hook/shared/utils/utils-functions.h"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include "HMUI/Touchable.hpp"

#include "string"

using namespace QuestUI;
using namespace HMUI;

float defJD = 0;
float reactJD = 0;
QuestUI::SliderSetting* JDSlider = nullptr;
UnityEngine::UI::Button* defJDButton = nullptr;
UnityEngine::UI::Button* reactJDButton = nullptr;

void DidActivate(ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(firstActivation) {
        self->get_gameObject()->AddComponent<Touchable*>();
        UnityEngine::GameObject* container = BeatSaberUI::CreateScrollableSettingsContainer(self->get_transform());

        AddConfigValueIncrementFloat(container->get_transform(), getModConfig().ReactTime, 1, 0.1, 0.1, 2.0);
        
        AddConfigValueToggle(container->get_transform(), getModConfig().AutoReact);
    }
}

UnityEngine::UI::HorizontalLayoutGroup* new_horizontal(UnityEngine::Transform* parent) {
    UnityEngine::UI::HorizontalLayoutGroup* layout = BeatSaberUI::CreateHorizontalLayoutGroup(parent);
    
    layout->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    layout->set_childForceExpandHeight(true);
    layout->GetComponent<UnityEngine::UI::LayoutElement*>()->set_minHeight(8);
    layout->set_spacing(5);

    return layout;
}

void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation) {
    if(firstActivation) {
        UnityEngine::UI::VerticalLayoutGroup* vertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject->get_transform());
        
        // increase container height
        vertical->get_rectTransform()->set_anchoredPosition({0, 10});
        
        // njs setting slider
        auto horizontal = new_horizontal(vertical->get_transform());

        JDSlider = BeatSaberUI::CreateSliderSetting(horizontal->get_transform(), getModConfig().JumpDist.GetName(), 0.1, getModConfig().JumpDist.GetValue(), 0.1, 50.0, 0.1, 
        [=](float value) {
            if(value > 0)
                getModConfig().JumpDist.SetValue(value);
        });
        BeatSaberUI::AddHoverHint(JDSlider->get_gameObject(), getModConfig().JumpDist.GetHoverHint());
        // make it bigger
        reinterpret_cast<UnityEngine::RectTransform*>(JDSlider->slider->get_transform())->set_sizeDelta({60, 0});

        // reaction time
        horizontal = new_horizontal(vertical->get_transform());
        
        BeatSaberUI::CreateText(horizontal->get_transform(), to_utf16("Reaction Time Jump Distance"));
        std::string buttonText = reactJD > 0 ? string_format("%.1f", reactJD) : "N/A";
        reactJDButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), to_utf16(buttonText), "PracticeButton", [=]() {
            // getLogger().debug("Setting slider");
            if(reactJD > 0) {
                JDSlider->set_value(reactJD);
                getModConfig().JumpDist.SetValue(reactJD);
            }
        });

        // default level jump distance
        horizontal = new_horizontal(vertical->get_transform());

        BeatSaberUI::CreateText(horizontal->get_transform(), to_utf16("Level Jump Distance"));
        buttonText = defJD > 0 ? string_format("%.1f", defJD) : "N/A";
        defJDButton = BeatSaberUI::CreateUIButton(horizontal->get_transform(), to_utf16(buttonText), "PracticeButton", [=]() {
            // getLogger().debug("Setting slider");
            if(defJD > 0) {
                JDSlider->set_value(defJD);
                getModConfig().JumpDist.SetValue(defJD);
            }
        });
    }
}

void changeDefJD(float value) {
    if(value <= 0)
        return;
    defJD = value;

    if(defJDButton)
        BeatSaberUI::SetButtonText(defJDButton, to_utf16(string_format("%.1f", value)));

    if(getModConfig().AutoDef.GetValue()) {
        if(JDSlider)
            JDSlider->set_value(value);
        getModConfig().JumpDist.SetValue(value);
    }
}

void changeReactJD(float value) {
    if(value <= 0)
        return;
    reactJD = value;

    if(reactJDButton)
        BeatSaberUI::SetButtonText(reactJDButton, to_utf16(string_format("%.1f", value)));

    if(getModConfig().AutoReact.GetValue()) {
        if(JDSlider)
            JDSlider->set_value(value);
        getModConfig().JumpDist.SetValue(value);
    }
}