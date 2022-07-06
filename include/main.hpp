#pragma once

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"

Logger& getLogger();
ModInfo& getModInfo();

#include "HMUI/ViewController.hpp"
#include "GlobalNamespace/GameplaySetupViewController.hpp"

void SetToLevelDefaults();

// gameplay menu config
void GameplaySettings(UnityEngine::GameObject* gameObject, bool firstActivation);
