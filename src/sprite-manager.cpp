#include "../extern/beatsaber-hook/shared/utils/logging.hpp"
#include "../include/sprite-manager.hpp"
#include "../include/utils.hpp"
#include "../include/config.hpp"
#include <map>
#include <string>
#include <string_view>
#include <optional>
#include "../extern/beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "../extern/beatsaber-hook/shared/utils/utils-functions.h"

std::unordered_map<std::string, sprite_pair_t> SpriteManager::sprites;

void SpriteManager::textureLoaded(texture_complete_t* completeWrapper) {
    ASSERT_MSG(completeWrapper, "texture_complete_t is nullptr in SpriteManager::textureLoaded!");
    auto content = il2cpp_utils::RunMethod("UnityEngine.Networking", "DownloadHandlerTexture", "GetContent", completeWrapper->webRequest).value_or(nullptr);
    ASSERT_MSG(content, "Failed to get texture content, or it was null!");
    getLogger().info("Finalizing load for texture: %s", completeWrapper->path.data());
    auto val = sprites.find(completeWrapper->path);
    ASSERT_MSG(val != sprites.end(), "Could not find completeWrapper path when finalizing textureLoaded!");
    auto width = RET_V_UNLESS(il2cpp_utils::GetPropertyValue<int>(content, "width"));
    auto height = RET_V_UNLESS(il2cpp_utils::GetPropertyValue<int>(content, "height"));
    auto rect = (Rect){0.0f, 0.0f, (float)width, (float)height};
    auto pivot = (Vector2){0.5f, 0.5f};
    // Sprite sprite = Sprite.Create(content, rect, pivot, 1024f, 1u, 0, Vector4.zero, false);
    Vector4 zero = (Vector4){0, 0, 0, 0};
    auto sprite = RET_V_UNLESS(il2cpp_utils::RunMethod("UnityEngine", "Sprite", "Create", content, rect, pivot, 1024.0f, 1u, 0, zero, false).value_or(nullptr));
    sprites[completeWrapper->path].loaded = true;
    sprites[completeWrapper->path].sprite = sprite;
    // Call DontDestroyOnLoad
    ASSERT_MSG(il2cpp_utils::RunMethod("UnityEngine", "Object", "DontDestroyOnLoad", sprite), "Failed to call UnityEngine.Object.DontDestroyOnLoad(sprite)");
    // Destruct created structure after
    completeWrapper->~texture_complete_t();
}

void SpriteManager::Initialize(HSVConfig config) {
    for (auto s : config.GetAllImagePaths()) {
        if (!LoadTexture(s)) {
            getLogger().warning("Could not load texture file: %s skipping it!", s.data());
            continue;
        }
        // Delay, if necessary (?)
        usleep(100000);
    }
}

void SpriteManager::Clear() {
    sprites.clear();
}

std::optional<Il2CppObject*> SpriteManager::GetSprite(std::string path) {
    auto found = sprites.find(path);
    if (found == sprites.end()) {
        // We didn't find it. Let's attempt to load it so when we try to get it later we get it.
        getLogger().debug("Attempting to load texture: %s", path.data());
        RET_NULLOPT_UNLESS(LoadTexture(path));
        return std::nullopt;
    }
    if (!found->second.loaded) {
        // It isn't loaded, but it exists. We need to wait for it to be loaded.
        getLogger().info("Waiting for texture at: %s to load before returning a valid one.", path.data());
        return std::nullopt;
    }
    getLogger().debug("Got sprite! Path: %s", path.data());
    return found->second.sprite;
}

bool SpriteManager::LoadTexture(std::string path) {
    if (sprites.find(path) != sprites.end()) {
        getLogger().info("Sprite: %s already exists!", path.data());
        return true;
    }
    getLogger().info("Loading texure: %s", path.data());
    if (!fileexists(path.data())) {
        getLogger().error("File: %s does not exist!", path.data());
        return false;
    }
    auto requestPath = il2cpp_utils::createcsstr(std::string("file:///") + path);
    auto webRequest = il2cpp_utils::RunMethod("UnityEngine.Networking", "UnityWebRequestTexture", "GetTexture", requestPath).value_or(nullptr);
    RET_F_UNLESS(webRequest);
    // Create completeWrapper
    getLogger().debug("Performing malloc for texture_complete_t!");
    texture_complete_t* completeWrapper = new texture_complete_t();
    getLogger().debug("malloc success: %p", completeWrapper);
    completeWrapper->path = path;
    completeWrapper->webRequest = webRequest;
    // Create completion action
    // TODO: Check to see if this happens too slowly, perhaps we need to create the action before sending the request?
    getLogger().debug("Sending web request...");
    auto asyncOperation = RET_F_UNLESS(il2cpp_utils::RunMethod(webRequest, "SendWebRequest").value_or(nullptr));
    getLogger().debug("Web request sent!");
    auto field = RET_F_UNLESS(il2cpp_utils::FindField(asyncOperation, "m_completeCallback"));
    auto action = RET_F_UNLESS(il2cpp_utils::MakeAction(field, completeWrapper, textureLoaded));
    if (!il2cpp_utils::SetFieldValue(asyncOperation, field, action))
    {
        return false;
    }
    getLogger().info("Began loading texture!");
    sprites[path] = {false, nullptr};
    return true;
}