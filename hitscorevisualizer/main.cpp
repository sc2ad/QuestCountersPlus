#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <wchar.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include "../beatsaber-hook/shared/inline-hook/inlineHook.h"

#define MOD_ID "HitScoreVisualizer"
#define VERSION "1.9.0"

#define LOG_LEVEL CRITICAL | ERROR | WARNING | INFO | DEBUG

#include "../beatsaber-hook/shared/utils/utils.h"
#include "../beatsaber-hook/rapidjson/include/rapidjson/document.h"
#include "../beatsaber-hook/rapidjson/include/rapidjson/allocators.h"
#include "main.h"

#define HandleSaberAfterCutSwingRatingCounterDidChangeEvent_offset 0x51CCAC

Config config;

void addJudgement(rapidjson::MemoryPoolAllocator<> &alloc, rapidjson::GenericArray<false, rapidjson::Value> arr, int thresh, std::string_view text, std::vector<float> colors, bool fade = false) {
    auto v = rapidjson::Value(rapidjson::kObjectType);
    v.AddMember("threshold", rapidjson::Value(thresh), alloc);
    v.AddMember("text", rapidjson::Value(text.data(), text.length()), alloc);
    auto color = rapidjson::Value(rapidjson::kArrayType);
    for (int i = 0; i < 4; i++) {
        color.PushBack(colors[i], alloc);
    }
    v.AddMember("color", color, alloc);
    v.AddMember("fade", rapidjson::Value(fade), alloc);
    arr.PushBack(v, alloc);
}

void addSegment(rapidjson::MemoryPoolAllocator<> &alloc, rapidjson::GenericArray<false, rapidjson::Value> arr, int thresh, std::string_view text) {
    auto v = rapidjson::Value(rapidjson::kObjectType);
    v.AddMember("threshold", rapidjson::Value(thresh), alloc);
    v.AddMember("text", rapidjson::Value(text.data(), text.length()), alloc);
    arr.PushBack(v, alloc);
}

void createdefaultjson() {
    rapidjson::Document& doc = Configuration::Load();
    rapidjson::MemoryPoolAllocator<> &alloc = doc.GetAllocator();
    doc.RemoveAllMembers();
    doc.AddMember("majorVersion", rapidjson::Value(2), alloc);
    doc.AddMember("minorVersion", rapidjson::Value(2), alloc);
    doc.AddMember("patchVersion", rapidjson::Value(0), alloc);
    auto judgements = rapidjson::Value(rapidjson::kArrayType).GetArray();
    addJudgement(alloc, judgements, 115, "%BFantastic%A%n%s", {1.0, 1.0, 1.0, 1.0});
    addJudgement(alloc, judgements, 101, "<size=80%>%BExcellent%A</size>%n%s", {0.0, 1.0, 0.0, 1.0});
    addJudgement(alloc, judgements, 90, "<size=80%>%BGreat%A</size>%n%s", {1.0, 0.980392158, 0.0, 1.0});
    addJudgement(alloc, judgements, 80, "<size=80%>%BGood%A</size>%n%s", {1.0, 0.6, 0.0, 1.0}, true);
    addJudgement(alloc, judgements, 60, "<size=80%>%BDecent%A</size>%n%s", {1.0, 0.0, 0.0, 1.0}, true);
    addJudgement(alloc, judgements, 0, "<size=80%>%BWay Off%A</size>%n%s", {0.5, 0.0, 0.0, 1.0}, true);
    doc.AddMember("judgements", judgements, alloc);
    auto beforeCutAngleJudgements = rapidjson::Value(rapidjson::kArrayType).GetArray();
    addSegment(alloc, beforeCutAngleJudgements, 70, "+");
    addSegment(alloc, beforeCutAngleJudgements, 0, " ");
    doc.AddMember("beforeCutAngleJudgements", beforeCutAngleJudgements, alloc);
    auto accuracyJudgements = rapidjson::Value(rapidjson::kArrayType).GetArray();
    addSegment(alloc, accuracyJudgements, 15, "+");
    addSegment(alloc, accuracyJudgements, 0, " ");
    doc.AddMember("accuracyJudgements", accuracyJudgements, alloc);
    auto afterCutAngleJudgements = rapidjson::Value(rapidjson::kArrayType).GetArray();
    addSegment(alloc, afterCutAngleJudgements, 30, "+");
    addSegment(alloc, afterCutAngleJudgements, 0, " ");
    doc.AddMember("afterCutAngleJudgements", afterCutAngleJudgements, alloc);
    doc.AddMember("useJson", rapidjson::Value(true), alloc);

    Configuration::Write();

    log(INFO, "Created default JSON config!");
}

void createdefault() {
    config.judgements.reserve(6);
    // Creating Fantastic judgement
    config.judgements[0] = {115, 1.0f, 1.0f, 1.0f, 1.0f, "Moon Struck!", false};
    // Creating Excellent judgement
    judgement excellent = 
    config.judgements[1] = {101, 0.0f, 1.0f, 0.0f, 1.0f, "<size=80%>Sugar Crush!</size>", true};
    // Creating Great judgement
    judgement great = {90, 1.0f, 0.980392158f, 0.0f, 1.0f, "<size=80%>Divine</size>", true};
    config.judgements[2] = great;
    // Creating Good judgement
    judgement good = {80, 1.0f, 0.6f, 0.0f, 1.0f, "<size=80%>Delicious</size>", true};
    config.judgements[3] = good;
    // Creating Decent judgement
    judgement decent = {60, 1.0f, 0.0f, 0.0f, 1.0f, "<size=80%>Tasty</size>", true};
    config.judgements[4] = decent;
    // Creating Way Off judgement
    judgement way_off = {0, 0.5f, 0.0f, 0.0f, 1.0f, "<size=80%>Sweet</size>", true};
    config.judgements[5] = way_off;
    // Set displaymode
    config.displayMode = DISPLAY_MODE_TEXTONTOP;
    // Set BeforeCutSegments
    config.beforeCutAngleJudgements.reserve(2);
    config.beforeCutAngleJudgements[0] = {70, "+"};
    config.beforeCutAngleJudgements[1] = {0, " "};
    // Set AccuracySegments
    config.accuracyJudgements.reserve(2);
    config.accuracyJudgements[0] = {15, "+"};
    config.accuracyJudgements[1] = {0, " "};
    // Set AfterCut
    config.afterCutAngleJudgements.reserve(2);
    config.afterCutAngleJudgements[0] = {30, "+"};
    config.afterCutAngleJudgements[1] = {0, " "};

    log(DEBUG, "Created default judgements!");
}

bool createjudgements(rapidjson::GenericArray<false, rapidjson::Value> arr) {
    int index = 0;
    config.judgements.reserve(arr.Capacity());
    for (auto& v : arr) {
        if (!v.IsObject()) {
            // ERROR
            return false;
        }
        for (auto jitr = v.GetObject().MemberBegin(); jitr != v.GetObject().MemberEnd(); jitr++) {
            if (strcmp(jitr->name.GetString(), "threshold") == 0) {
                if (!jitr->value.IsInt()) {
                    // ERROR
                    return false;
                }
                config.judgements[index].threshold = jitr->value.GetInt();
            }
            else if (strcmp(jitr->name.GetString(), "text") == 0) {
                if (!jitr->value.IsString()) {
                    // ERROR
                    return false;
                }
                config.judgements[index].text = jitr->value.GetString();
            }
            else if (strcmp(jitr->name.GetString(), "color") == 0) {
                if (!jitr->value.IsArray()) {
                    // ERROR
                    return false;
                }
                int ci = 0;
                for (auto& c : jitr->value.GetArray()) {
                    if (!c.IsFloat()) {
                        // ERROR
                        return false;
                    }
                    switch (ci) {
                        case 0: 
                            config.judgements[index].r = c.GetFloat();
                            break;
                        case 1:
                            config.judgements[index].g = c.GetFloat();
                            break;
                        case 2:
                            config.judgements[index].b = c.GetFloat();
                            break;
                        case 3:
                            config.judgements[index].a = c.GetFloat();
                            break;
                    }
                    ci++;
                }
            }
            else if (strcmp(jitr->name.GetString(), "fade") == 0) {
                if (!jitr->value.IsBool()) {
                    // ERROR
                    return false;
                }
                config.judgements[index].fade = jitr->value.GetBool();
            }
        }
        index++;
    }
    return true;
}

bool createjudgementsegments(std::vector<judgement_segment> vec, rapidjson::GenericArray<false, rapidjson::Value> arr) {
    int index = 0;
    vec.reserve(arr.Capacity());
    for (auto& v : arr) {
        if (!v.IsObject()) {
            // ERROR
            return false;
        }
        for (auto jitr = v.GetObject().MemberBegin(); jitr != v.GetObject().MemberEnd(); jitr++) {
            if (strcmp(jitr->name.GetString(), "threshold") == 0) {
                if (!jitr->value.IsInt()) {
                    // ERROR
                    return false;
                }
                vec[index].threshold = jitr->value.GetInt();
            }
            else if (strcmp(jitr->name.GetString(), "text") == 0) {
                if (!jitr->value.IsString()) {
                    // ERROR
                    return false;
                }
                vec[index].text = jitr->value.GetString();
            }
        }
        index++;
    }
    return true;
}

// Returns 0 on success, -1 on failure, but don't create default JSON, -2 on failure and do create JSON
int loadjudgements() {
    rapidjson::Document& json_doc = Configuration::Load();

    // Two approach ideas:
    // 1. Iterate over all members
    // 2. Call FindMember a bunch of times
    // 1:
    if (json_doc.HasMember("useJson")) {
        if (!json_doc["useJson"].IsBool()) {
            json_doc["useJson"].SetBool(true);
            Configuration::Write();
        }
        if (!json_doc["useJson"].GetBool()) {
            // Exit without parsing the JSON
            log(INFO, "useJson is false, loading candy crush config!");
            return -1;
        }
    }
    for (auto itr = json_doc.MemberBegin(); itr != json_doc.MemberEnd(); itr++) {
        if (strcmp(itr->name.GetString(), "majorVersion") == 0) {
            if (!itr->value.IsInt()) {
                // ERROR
                return -2;
            }
            config.majorVersion = itr->value.GetInt();
        }
        else if (strcmp(itr->name.GetString(), "minorVersion") == 0) {
            if (!itr->value.IsInt()) {
                // ERROR
                return -2;
            }
            config.minorVersion = itr->value.GetInt();
        }
        else if (strcmp(itr->name.GetString(), "patchVersion") == 0) {
            if (!itr->value.IsInt()) {
                // ERROR
                return -2;
            }
            config.patchVersion = itr->value.GetInt();
        }
        else if (strcmp(itr->name.GetString(), "displayMode") == 0) {
            if (!itr->value.IsString()) {
                // ERROR
                return -2;
            }
            if (strcmp(itr->value.GetString(), "format")) {
                config.displayMode = DISPLAY_MODE_FORMAT;
            }
            else if (strcmp(itr->value.GetString(), "numeric") == 0) {
                config.displayMode = DISPLAY_MODE_NUMERIC;
            }
            else if (strcmp(itr->value.GetString(), "scoreOnTop") == 0) {
                config.displayMode = DISPLAY_MODE_SCOREONTOP;
            }
            else if (strcmp(itr->value.GetString(), "textOnly") == 0) {
                config.displayMode = DISPLAY_MODE_TEXTONLY;
            }
            else if (strcmp(itr->value.GetString(), "textOnTop") == 0) {
                config.displayMode = DISPLAY_MODE_TEXTONTOP;
            } else {
                return -2;
            }
        }
        else if (strcmp(itr->name.GetString(), "judgements") == 0) {
            if (!itr->value.IsArray()) {
                // ERROR
                return -2;
            }
            if (!createjudgements(itr->value.GetArray())) {
                // ERROR
                return -2;
            }
        }
        else if (strcmp(itr->name.GetString(), "beforeCutAngleJudgements") == 0) {
            if (!itr->value.IsArray()) {
                // ERROR
                return -2;
            }
            if (!createjudgementsegments(config.beforeCutAngleJudgements, itr->value.GetArray())) {
                // ERROR
                return -2;
            }
        }
        else if (strcmp(itr->name.GetString(), "accuracyJudgements") == 0) {
            if (!itr->value.IsArray()) {
                // ERROR
                return -2;
            }
            if (!createjudgementsegments(config.accuracyJudgements, itr->value.GetArray())) {
                // ERROR
                return -2;
            }
        }
        else if (strcmp(itr->name.GetString(), "afterCutAngleJudgements") == 0) {
            if (!itr->value.IsArray()) {
                // ERROR
                return -2;
            }
            if (!createjudgementsegments(config.afterCutAngleJudgements, itr->value.GetArray())) {
                // ERROR
                return -2;
            }
        }
    }
    if (config.judgements.capacity() == 0) {
        // DID NOT LOAD JUDGEMENTS
        log(INFO, "Did not load all required information from JSON. Empty config file?");
        return -2;
    }
    if (config.majorVersion < 2 || (config.majorVersion == 2 && config.minorVersion < 2) || (config.majorVersion == 2 && config.minorVersion == 2 && config.patchVersion < 0)) {
        // VERSION ERROR
        log(INFO, "Version mismatch! Version is: %i.%i.%i but should be >= 2.2.0!", config.majorVersion, config.minorVersion, config.patchVersion);
        return -1;
    }
    return 0;
}

const char* getBestSegment(std::vector<judgement_segment> segments, int comparison) {
    judgement_segment best = segments[segments.capacity() - 1];
    for (int i = segments.capacity() - 2; i >= 0; i--) {
        if (segments[i].threshold > comparison) {
            break;
        }
        best = segments[i];
    }
    return best.text;
}

bool __cached = false;
Il2CppClass* tmp_class;
const MethodInfo* set_richText;
const MethodInfo* set_enableWordWrapping;
const MethodInfo* set_overflowMode;
const MethodInfo* get_text;
const MethodInfo* set_text;
Il2CppClass* str_class;
const MethodInfo* concat;
const MethodInfo* replace;

Il2CppString* replaceBuffer(Il2CppString* judgement_cs, std::string_view left, std::string_view right) {
    void* args[] = {reinterpret_cast<void*>(il2cpp_utils::createcsstr(left)), reinterpret_cast<void*>(il2cpp_utils::createcsstr(right))};
    Il2CppException* exp;
    judgement_cs = (Il2CppString*)il2cpp_functions::runtime_invoke(replace, judgement_cs, args, &exp);
    if (exp) {
        // ERROR VIA EXCEPTION
        log(ERROR, "%s", il2cpp_utils::ExceptionToString(exp).c_str());
        return nullptr;
    }
    return judgement_cs;
}

Il2CppString* concatBuffer(Il2CppString* left, std::string_view right) {
    void* args[] = {reinterpret_cast<void*>(left), reinterpret_cast<void*>(il2cpp_utils::createcsstr(right))};
    Il2CppException* exp;
    Il2CppString* concatted = (Il2CppString*)il2cpp_functions::runtime_invoke(replace, nullptr, args, &exp);
    if (exp) {
        // ERROR VIA EXCEPTION
        log(ERROR, "%s", il2cpp_utils::ExceptionToString(exp).c_str());
        return nullptr;
    }
    return concatted;
}

Il2CppString* concatBuffer(Il2CppString* left, Il2CppString* right) {
    void* args[] = {reinterpret_cast<void*>(left), reinterpret_cast<void*>(right)};
    Il2CppException* exp;
    Il2CppString* concatted = (Il2CppString*)il2cpp_functions::runtime_invoke(replace, nullptr, args, &exp);
    if (exp) {
        // ERROR VIA EXCEPTION
        log(ERROR, "%s", il2cpp_utils::ExceptionToString(exp).c_str());
        return nullptr;
    }
    return concatted;
}

void checkJudgements(FlyingScoreEffect* scorePointer, int beforeCut, int afterCut, int cutDistance) {
    if (!__cached) {
        tmp_class = il2cpp_utils::GetClassFromName("TMPro", "TMP_Text");
        set_richText = il2cpp_functions::class_get_method_from_name(tmp_class, "set_richText", 1);
        set_enableWordWrapping = il2cpp_functions::class_get_method_from_name(tmp_class, "set_enableWordWrapping", 1);
        set_overflowMode = il2cpp_functions::class_get_method_from_name(tmp_class, "set_overflowMode", 1);
        get_text = il2cpp_functions::class_get_method_from_name(tmp_class, "get_text", 0);
        set_text = il2cpp_functions::class_get_method_from_name(tmp_class, "set_text", 1);
        str_class = il2cpp_utils::GetClassFromName("System", "String");
        // TODO MAKE THESE NOT USE PARAM COUNT
        const MethodInfo* current;
        void* myIter;
        while ((current = il2cpp_functions::class_get_methods(str_class, &myIter))) {
            if (current->parameters_count != 2) {
                continue;
            }
            for (int i = 0; i < current->parameters_count; i++) {
                if (!il2cpp_functions::type_equals(current->parameters[i].parameter_type, il2cpp_functions::class_get_type_const(str_class))) {
                    goto next_method;
                }
            }
            if (strcmp(current->name, "Concat") == 0) {
                concat = current;
            } else if (strcmp(current->name, "Replace") == 0) {
                replace = current;
            }
            next_method:;
        }
    }
    int score = beforeCut + afterCut;
    log(DEBUG, "Checking judgements for score: %i", score);
    judgement best = config.judgements[config.judgements.capacity() - 1];
    for (int i = config.judgements.capacity()-2; i >= 0; i--) {
        if (config.judgements[i].threshold > score) {
            break;
        }
        best = config.judgements[i];
    }
    log(DEBUG, "Setting score effect's color to best color with threshold: %i for score: %i", best.threshold, score);
    // TODO Add fading
    scorePointer->color.r = best.r;
    scorePointer->color.g = best.g;
    scorePointer->color.b = best.b;
    scorePointer->color.a = best.a;
    log(DEBUG, "Modified color!");

    // Runtime invoke set_richText to true
    bool set_to = true;
    if (!il2cpp_utils::RunMethod(scorePointer->text, set_richText, &set_to)) {
        // ERROR VIA EXCEPTION
        return;
    }

    // Runtime invoke set_enableWordWrapping false
    set_to = false;
    if (!il2cpp_utils::RunMethod(scorePointer->text, set_enableWordWrapping, &set_to)) {
        // ERROR VIA EXCEPTION
        return;
    }

    // Runtime invoke set_overflowMode false
    if (!il2cpp_utils::RunMethod(scorePointer->text, set_overflowMode, &set_to)) {
        // ERROR VIA EXCEPTION
        return;
    }

    // Get Text
    void* args[] = {};
    Il2CppException* exp;
    Il2CppString* old_text = (Il2CppString*)il2cpp_functions::runtime_invoke(get_text, scorePointer->text, args, &exp);
    if (exp) {
        // ERROR VIA EXCEPTION
        log(ERROR, "%s", il2cpp_utils::ExceptionToString(exp).c_str());
        return;
    }

    Il2CppString* judgement_cs = il2cpp_utils::createcsstr(best.text);

    if (config.displayMode == DISPLAY_MODE_FORMAT) {
        // THIS IS VERY INEFFICIENT AND SLOW BUT SHOULD WORK!
        log(DEBUG, "Displaying formated text!");
        char buffer[4]; // Max length for score buffers is 3
        // %b
        sprintf(buffer, "%i", beforeCut);
        judgement_cs = replaceBuffer(judgement_cs, "%b", buffer);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %c
        buffer[1] = '\0'; buffer[2] = '\0'; // Reset buffer
        sprintf(buffer, "%i", cutDistance);
        judgement_cs = replaceBuffer(judgement_cs, "%c", buffer);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %a
        buffer[1] = '\0'; buffer[2] = '\0'; // Reset buffer
        sprintf(buffer, "%i", afterCut);
        judgement_cs = replaceBuffer(judgement_cs, "%a", buffer);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %B
        const char* bestBeforeSeg = getBestSegment(config.beforeCutAngleJudgements, beforeCut);
        judgement_cs = replaceBuffer(judgement_cs, "%B", bestBeforeSeg);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %C
        const char* bestCutAcc = getBestSegment(config.accuracyJudgements, cutDistance);
        judgement_cs = replaceBuffer(judgement_cs, "%C", bestCutAcc);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %A
        const char* bestAfterSeg = getBestSegment(config.afterCutAngleJudgements, afterCut);
        judgement_cs = replaceBuffer(judgement_cs, "%A", bestAfterSeg);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %s
        buffer[1] = '\0'; buffer[2] = '\0'; // Reset buffer
        sprintf(buffer, "%i", score);
        judgement_cs = replaceBuffer(judgement_cs, "%s", buffer);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %p
        char percentBuff[7]; // 6 is upper bound for 100.00 percent
        sprintf(percentBuff, "%.2f", score / 115.0 * 100.0);
        judgement_cs = replaceBuffer(judgement_cs, "%p", percentBuff);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %%
        judgement_cs = replaceBuffer(judgement_cs, "%b", "%");
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
        // %n
        judgement_cs = replaceBuffer(judgement_cs, "%b", "\n");
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
    }
    else if (config.displayMode == DISPLAY_MODE_NUMERIC) {
        // Numeric display ONLY
        log(DEBUG, "Displaying numeric text ONLY!");
        judgement_cs = old_text;
    }
    else if (config.displayMode == DISPLAY_MODE_SCOREONTOP) {
        // Score on top
        log(DEBUG, "Displaying score on top!");
        // Add newline
        judgement_cs = concatBuffer(concatBuffer(old_text, "\n"), best.text);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
    }
    else {
        // Text on top
        log(DEBUG, "Displaying judgement text on top!");
        // Add newline
        judgement_cs = concatBuffer(concatBuffer(judgement_cs, "\n"), old_text);
        if (!judgement_cs) {
            // ERROR VIA EXCEPTION
            return;
        }
    }

    if (!il2cpp_utils::RunMethod(scorePointer->text, set_text, judgement_cs)) {
        // ERROR VIA EXCEPTION
        return;
    }
}

void loadall() {
    int r = loadjudgements();
    if (r == -2) {
        createdefaultjson();
        log(INFO, "Loading default JSON...");
        r = loadjudgements();
    } if (r == -1) {
        log(INFO, "Loading candy crush config...");
        createdefault();
    } if (r == -2) {
        log(CRITICAL, "COULD NOT LOAD DEFAULT JSON!");
    }
}

MAKE_HOOK(HandleSaberAfterCutSwingRatingCounterDidChangeEvent, HandleSaberAfterCutSwingRatingCounterDidChangeEvent_offset, void, FlyingScoreEffect* self, void* saberAfterCutSwingRatingCounter, float rating) {
    log(DEBUG, "Called HandleSaberAfterCutSwingRatingCounterDidChangeEvent Hook!");
    log(DEBUG, "Attempting to call standard HandleSaberAfterCutSwingRatingCounterDidChangeEvent...");
    HandleSaberAfterCutSwingRatingCounterDidChangeEvent(self, saberAfterCutSwingRatingCounter, rating);
    auto klass = il2cpp_utils::GetClassFromName("", "ScoreController");
    if (!klass) {
        log(CRITICAL, "Could not find ScoreController class!");
        return;
    }
    auto rawScoreWithoutMultiplier = il2cpp_functions::class_get_method_from_name(klass, "RawScoreWithoutMultiplier", 5);
    if (!rawScoreWithoutMultiplier) {
        log(CRITICAL, "Could not find ScoreController.RawScoreWithoutMultiplier method! (with 5 params)");
        il2cpp_utils::LogClass(klass);
        return;
    }
    int beforeCut = 0;
    int afterCut = 0;
    int cutDistance = 0;
    if (!il2cpp_utils::RunMethod(nullptr, rawScoreWithoutMultiplier, self->noteCutInfo, self->saberAfterCutSwingRatingCounter, &beforeCut, &afterCut, &cutDistance)) {
        // ERROR FROM EXCEPTION
        return;
    }
    int score = beforeCut + afterCut;
    log(DEBUG, "RawScore: %i", score);
    log(DEBUG, "Checking judgements...");
    checkJudgements(self, beforeCut, afterCut, cutDistance);
    log(DEBUG, "Completed HandleSaberAfterCutSwingRatingCounterDidChangeEvent!");
}

__attribute__((constructor)) void lib_main()
{
    log(DEBUG, "Installing HitScoreVisualizer...");
    il2cpp_functions::Init();
    // INSTALL_HOOK(init_and_present);
    // log("Installed InitAndPresent Hook!");
    INSTALL_HOOK(HandleSaberAfterCutSwingRatingCounterDidChangeEvent);
    log(DEBUG, "Installed HandleSaberAfterCutSwingRatingCounterDidChangeEvent Hook!");
    // Attempt to add and create judgements
    // Attempt to find judgements
    loadall();
}