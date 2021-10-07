#pragma once

#include "modloader/shared/modloader.hpp"

#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"

Logger& getLogger();

#include "HMUI/ViewController.hpp"
#include "GlobalNamespace/GameplaySetupViewController.hpp"

// config in mod settings
void DidActivate(HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);

// normal gameplay config
void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation);

// important values
void changeReactJD(float value);
void changeDefJD(float value);