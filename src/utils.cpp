#include "utils.hpp"

#include "GlobalNamespace/BeatmapBasicData.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "conditional-dependencies/shared/main.hpp"
#include "main.hpp"
#include "metacore/shared/game.hpp"
#include "metacore/shared/songs.hpp"

using namespace GlobalNamespace;

static float BoundDistance(float halfJumpDistance, float min, float max) {
    if (halfJumpDistance < min)
        return min;
    if (halfJumpDistance > max)
        return max;
    return halfJumpDistance;
}

static inline float BoundDuration(float halfJumpDuration, float minDist, float maxDist, float njs) {
    float halfJumpDistance = halfJumpDuration * njs;
    return BoundDistance(halfJumpDistance, minDist, maxDist) / njs;
}

static constexpr float startHalfJumpDurationInBeats = 4;
static constexpr float maxHalfJumpDistance = 18;

float JDFixer::GetDefaultHalfJumpDuration(float njs, float beatDuration, float startBeatOffset) {
    // transforms into duration in beats, note jump speed in beats
    float halfJumpDuration = startHalfJumpDurationInBeats;
    float distancePerBeat = njs * beatDuration;
    // could be do-while. smh my head, beat games
    float halfJumpDistance = distancePerBeat * halfJumpDuration;
    while (halfJumpDistance > maxHalfJumpDistance - 0.001) {
        halfJumpDuration /= (float) 2;
        halfJumpDistance = distancePerBeat * halfJumpDuration;
    }
    // this is where the "dynamic" jump distance setting is added or subtracted
    halfJumpDuration += startBeatOffset;
    // keeps it from being too low, but is flawed in my opinion:
    // it uses duration in beats, making the minimum too large if a map has a low enough bpm
    if (halfJumpDuration < 0.25)
        halfJumpDuration = 0.25;
    // turn back into duration in seconds, instead of in beats
    return halfJumpDuration * beatDuration;
}

float JDFixer::GetNJSModifier(float speedModifier) {
    static auto RebeatEnabled = CondDeps::Find<bool>("rebeat", "GetEnabled");
    if (!RebeatEnabled || !RebeatEnabled.value()())
        return speedModifier;
    switch ((int) (speedModifier * 10)) {
        case 12:
            return 1.1;
        case 15:
            return 1.3;
        default:
            return 1;
    }
}

static std::vector<std::pair<std::string, int>> GetAllBeatmaps(GlobalNamespace::BeatmapKey beatmap) {
    std::vector<std::pair<std::string, int>> ret;

    auto keys = MetaCore::Songs::FindLevel(beatmap)->GetBeatmapKeys();

    for (auto& beatmap : ArrayW<BeatmapKey>(keys)) {
        std::string name = beatmap.beatmapCharacteristic->serializedName;
        int diff = (int) beatmap.difficulty;
        ret.emplace_back(name, diff);
    }

    return ret;
}

static void MigrateLevelPresets(GlobalNamespace::BeatmapKey beatmap) {
    std::string id = beatmap.levelId;
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(id);
    if (iter == map.end())
        return;

    // saved without difficulty/characteristic info, needs to be duplicated to all
    if (auto value = iter->second.GetValue<JDFixer::LevelPreset>()) {
        logger.debug("Duplicating unspecified level preset to all difficulties");

        StringKeyedMap<JDFixer::LevelPreset> newValue;
        auto allEntries = GetAllBeatmaps(beatmap);
        for (auto& [charName, diff] : allEntries)
            newValue[charName + std::to_string(diff)] = *value;

        iter->second.SetValue(newValue);
        getModConfig().Levels.SetValue(map);
    }
}

std::optional<std::pair<JDFixer::Indicator, JDFixer::LevelPreset>> JDFixer::GetLevelPreset(GlobalNamespace::BeatmapKey beatmap) {
    MigrateLevelPresets(beatmap);

    std::string id = beatmap.levelId;
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(id);
    if (iter == map.end())
        return std::nullopt;

    // search: characteristic ser. name + difficulty "Standard2" to preset
    std::string charDiff = beatmap.beatmapCharacteristic->serializedName;
    charDiff += std::to_string((int) beatmap.difficulty);

    // guaranteed to be a map after migrate
    auto charDiffs = *iter->second.GetValue<StringKeyedMap<LevelPreset>>();
    auto iter2 = charDiffs.find(charDiff);
    if (iter2 == charDiffs.end())
        return std::nullopt;

    Indicator ret = {
        .id = id,
        .map = charDiff,
    };
    return std::make_pair(ret, iter2->second);
}

static void SetLevelPresetInternal(std::string levelID, std::string mapID, JDFixer::LevelPreset value) {
    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(levelID);
    if (iter != map.end()) {
        auto charDiffs = *iter->second.GetValue<StringKeyedMap<JDFixer::LevelPreset>>();
        charDiffs[mapID] = value;
        iter->second.SetValue(charDiffs);
    } else
        map[levelID] = StringKeyedMap<JDFixer::LevelPreset>({{mapID, value}});

    getModConfig().Levels.SetValue(map);
}

void JDFixer::SetLevelPreset(GlobalNamespace::BeatmapKey beatmap, LevelPreset value) {
    MigrateLevelPresets(beatmap);

    std::string id = beatmap.levelId;
    std::string charDiff = beatmap.beatmapCharacteristic->serializedName;
    charDiff += std::to_string((int) beatmap.difficulty);

    SetLevelPresetInternal(id, charDiff, value);
}

void JDFixer::RemoveLevelPreset(GlobalNamespace::BeatmapKey beatmap) {
    MigrateLevelPresets(beatmap);

    std::string id = beatmap.levelId;
    std::string charDiff = beatmap.beatmapCharacteristic->serializedName;
    charDiff += std::to_string((int) beatmap.difficulty);

    auto map = getModConfig().Levels.GetValue();
    auto iter = map.find(id);
    if (iter == map.end())
        return;

    auto charDiffs = *iter->second.GetValue<StringKeyedMap<LevelPreset>>();
    auto iter2 = charDiffs.find(charDiff);
    if (iter2 == charDiffs.end())
        return;

    charDiffs.erase(iter2);
    iter->second.SetValue(charDiffs);
    getModConfig().Levels.SetValue(map);
}

static float GetDefaultDifficultyNJS(BeatmapDifficulty difficulty) {
    switch (difficulty) {
        case BeatmapDifficulty::Easy:
            return 10;
        case BeatmapDifficulty::Normal:
            return 10;
        case BeatmapDifficulty::Hard:
            return 10;
        case BeatmapDifficulty::Expert:
            return 12;
        case BeatmapDifficulty::ExpertPlus:
            return 16;
        default:
            return 5;
    }
}

JDFixer::Values JDFixer::GetLevelDefaults(GlobalNamespace::BeatmapKey beatmap, float speed) {
    if (!beatmap)
        return {};

    auto level = MetaCore::Songs::FindLevel(beatmap);
    float bpm = level->beatsPerMinute;
    auto data = level->GetDifficultyBeatmapData(beatmap.beatmapCharacteristic, beatmap.difficulty);

    float njs = data->noteJumpMovementSpeed;
    if (njs <= 0)
        njs = GetDefaultDifficultyNJS(beatmap.difficulty);

    float offset = data->noteJumpStartBeatOffset;

    float halfJumpDuration = GetDefaultHalfJumpDuration(njs, 60 / bpm, offset);
    njs *= speed;
    float halfJumpDistance = halfJumpDuration * njs;

    return Values{.halfJumpDuration = halfJumpDuration, .halfJumpDistance = halfJumpDistance, .njs = njs};
}

float JDFixer::GetNPS(BeatmapLevel* level, IReadonlyBeatmapData* data) {
    float length = level->songDuration;
    int noteCount = data->cuttableNotesCount;
    return noteCount / length;
}

float JDFixer::GetBPM(BeatmapKey level) {
    return MetaCore::Songs::FindLevel(level)->beatsPerMinute;
}

float JDFixer::GetValue(Values& values, int id) {
    switch (id) {
        case 1:
            return values.GetJumpDuration();
        case 2:
            return values.GetJumpDistance();
        case 3:
            return values.njs;
        default:
            return 0;
    }
}

bool JDFixer::ConditionMet(ConditionPreset const& check, float njs, float nps, float bpm) {
    bool matches = true;
    for (auto const& condition : check.Conditions) {
        float value;
        if (condition.Type == 0)
            value = nps;
        else if (condition.Type == 1)
            value = njs;
        else
            value = bpm;
        bool thisMatch;
        if (condition.Comparison == 0)
            thisMatch = value <= condition.Value;
        else
            thisMatch = value >= condition.Value;
        // basically, or takes proirity over and
        if (condition.AndOr == 0)
            matches = matches && thisMatch;
        else if (matches)
            return true;
        else
            matches = thisMatch;
    }
    return matches;
}

void JDFixer::Preset::UpdateLevelPreset() {
    SetLevelPresetInternal(internalLevelID, internalMapID, internalLevel);
}

void JDFixer::Preset::UpdateCondition() {
    auto presets = getModConfig().Presets.GetValue();
    presets[internalIdx] = internalCondition;
    getModConfig().Presets.SetValue(presets);
}

float JDFixer::Preset::Bound(float value) {
    if (!GetUseBounds())
        return value;
    if (GetUseDuration())
        return BoundDuration(value, GetBoundMin(), GetBoundMax(), GetNJS());
    else
        return BoundDistance(value, GetBoundMin(), GetBoundMax());
}

void JDFixer::Preset::SetMainValue(float value) {
    switch (type) {
        case Type::Main:
            if (getModConfig().UseDuration.GetValue())
                getModConfig().Duration.SetValue(value);
            else
                getModConfig().Distance.SetValue(value);
            break;
        case Type::Condition:
            if (internalCondition.UseDuration)
                internalCondition.Duration = value;
            else
                internalCondition.Distance = value;
            UpdateCondition();
            break;
        case Type::Level:
            internalLevel.MainValue = value;
            UpdateLevelPreset();
            break;
    }
}

float JDFixer::Preset::GetMainValue() {
    switch (type) {
        case Type::Main:
            if (getModConfig().UseDuration.GetValue())
                return getModConfig().Duration.GetValue();
            return getModConfig().Distance.GetValue();
        case Type::Condition:
            if (internalCondition.UseDuration)
                return internalCondition.Duration;
            return internalCondition.Distance;
        case Type::Level:
            return internalLevel.MainValue;
    }
}

void JDFixer::Preset::SetDuration(float value) {
    if (GetUseDuration())
        SetMainValue(value);
    else
        SetMainValue(value * GetNJS());
}

float JDFixer::Preset::GetDuration() {
    if (GetUseDuration())
        return Bound(GetMainValue());
    return Bound(GetMainValue()) / GetNJS();
}

void JDFixer::Preset::SetDistance(float value) {
    if (GetUseDuration())
        SetMainValue(value / GetNJS());
    else
        SetMainValue(value);
}

float JDFixer::Preset::GetDistance() {
    if (GetUseDuration())
        return Bound(GetMainValue()) * GetNJS();
    return Bound(GetMainValue());
}

void JDFixer::Preset::SetNJS(float value) {
    switch (type) {
        case Type::Main:
            getModConfig().NJS.SetValue(value);
            break;
        case Type::Condition:
            internalCondition.NJS = value;
            UpdateCondition();
            break;
        case Type::Level:
            internalLevel.NJS = value;
            UpdateLevelPreset();
            break;
    }
}

float JDFixer::Preset::GetNJS() {
    if (!GetOverrideNJS())
        return levelNJS;
    switch (type) {
        case Type::Main:
            return getModConfig().NJS.GetValue();
        case Type::Condition:
            return internalCondition.NJS;
        case Type::Level:
            return internalLevel.NJS;
    }
}

#define S_PROP(typ, name, cfgName, structName, levelRet, ...) \
void JDFixer::Preset::Set##name(typ value) { \
    switch(type) { \
    case Type::Main: \
        getModConfig().cfgName.SetValue(value); \
        break; \
    case Type::Condition: \
        internalCondition.structName = value; \
        UpdateCondition(); \
        break; \
    case Type::Level: \
        __VA_ARGS__; \
        break; \
    } \
} \
typ JDFixer::Preset::Get##name() { \
    switch(type) { \
    case Type::Main: \
        return getModConfig().cfgName.GetValue(); \
    case Type::Condition: \
        return internalCondition.structName; \
    case Type::Level: \
        return levelRet; \
    } \
}
#define PROP(typ, name, cfgName, structName) \
S_PROP(typ, name, cfgName, structName, internalLevel.structName, internalLevel.structName = value; UpdateLevelPreset())

PROP(bool, OverrideNJS, UseNJS, OverrideNJS);
PROP(bool, UseDuration, UseDuration, UseDuration);
S_PROP(bool, UseDefaults, AutoDef, SetToDefaults, false);
S_PROP(bool, UseBounds, BoundJD, DistanceBounds, false);
S_PROP(float, BoundMin, MinJD, DistanceMin, 0);
S_PROP(float, BoundMax, MaxJD, DistanceMax, 0);

void JDFixer::Preset::SetCondition(Condition value, int idx) {
    if (type == Type::Condition) {
        if (idx >= internalCondition.Conditions.size())
            internalCondition.Conditions.push_back(value);
        else
            internalCondition.Conditions[idx] = value;
        UpdateCondition();
    }
}

JDFixer::Condition JDFixer::Preset::GetCondition(int idx) {
    if (type == Type::Condition && idx < internalCondition.Conditions.size())
        return internalCondition.Conditions[idx];
    return {};
}

void JDFixer::Preset::RemoveCondition(int idx) {
    if (type == Type::Condition && idx < internalCondition.Conditions.size()) {
        internalCondition.Conditions.erase(internalCondition.Conditions.begin() + idx);
        UpdateCondition();
    }
}

int JDFixer::Preset::GetConditionCount() {
    if (type == Type::Condition)
        return internalCondition.Conditions.size();
    return 0;
}

int JDFixer::Preset::GetConditionPresetIndex() {
    if (type == Type::Condition)
        return internalIdx;
    return -1;
}

void JDFixer::Preset::SyncCondition(int idxChange) {
    if (type != Type::Condition)
        return;
    internalIdx += idxChange;
    internalCondition = getModConfig().Presets.GetValue()[internalIdx];
}

bool JDFixer::Preset::ShiftForward() {
    if (type != Type::Condition)
        return false;
    auto presets = getModConfig().Presets.GetValue();
    if (internalIdx == presets.size() - 1)
        return false;
    presets[internalIdx] = presets[internalIdx + 1];
    presets[internalIdx + 1] = internalCondition;
    internalIdx++;
    getModConfig().Presets.SetValue(presets);
    return true;
}

bool JDFixer::Preset::ShiftBackward() {
    if (type != Type::Condition || internalIdx == 0)
        return false;
    auto presets = getModConfig().Presets.GetValue();
    presets[internalIdx] = presets[internalIdx - 1];
    presets[internalIdx - 1] = internalCondition;
    internalIdx--;
    getModConfig().Presets.SetValue(presets);
    return true;
}

JDFixer::LevelPreset JDFixer::Preset::GetAsLevelPreset() {
    if (GetIsLevelPreset())
        return internalLevel;
    LevelPreset ret;
    ret.UseDuration = GetUseDuration();
    ret.MainValue = Bound(GetMainValue());
    ret.OverrideNJS = GetOverrideNJS();
    ret.NJS = GetNJS();
    return ret;
}

bool JDFixer::Preset::GetIsLevelPreset() {
    return type == Type::Level;
}

void JDFixer::Preset::UpdateLevel(Values const& levelValues) {
    levelNJS = levelValues.njs;
    if (!GetUseDefaults())
        return;
    if (GetUseDuration())
        SetMainValue(levelValues.GetJumpDuration());
    else
        SetMainValue(levelValues.GetJumpDistance());
    SetNJS(levelNJS);
}

JDFixer::Preset::Preset(std::string levelID, std::string mapID, Values const& levelValues) {
    type = Type::Level;
    internalLevelID = levelID;
    internalMapID = mapID;
    auto presets = getModConfig().Levels.GetValue()[levelID];
    auto maps = *presets.GetValue<StringKeyedMap<LevelPreset>>();
    internalLevel = maps[mapID];
    levelNJS = levelValues.njs;
}

JDFixer::Preset::Preset(int conditionIdx, Values const& levelValues, bool setToLevel) {
    type = Type::Condition;
    internalCondition = getModConfig().Presets.GetValue()[conditionIdx];
    internalIdx = conditionIdx;
    levelNJS = levelValues.njs;
    if (internalCondition.SetToDefaults && setToLevel) {
        if (internalCondition.UseDuration)
            internalCondition.Duration = levelValues.GetJumpDuration();
        else
            internalCondition.Distance = levelValues.GetJumpDistance();
        internalCondition.NJS = levelValues.njs;
        UpdateCondition();
    }
}

JDFixer::Preset::Preset(Values const& levelValues, bool setToLevel) {
    type = Type::Main;
    levelNJS = levelValues.njs;
    if (getModConfig().AutoDef.GetValue() && setToLevel) {
        if (getModConfig().UseDuration.GetValue())
            getModConfig().Duration.SetValue(levelValues.GetJumpDuration());
        else
            getModConfig().Distance.SetValue(levelValues.GetJumpDistance());
        getModConfig().NJS.SetValue(levelValues.njs);
    }
}

void JDFixer::UpdateScoreSubmission(bool overridingNJS) {
    MetaCore::Game::SetScoreSubmission(MOD_ID, !overridingNJS || getModConfig().Disable.GetValue());
}
