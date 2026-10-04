// WXL opcode bridge for the server-authoritative area-loot window.
// Copyright (C) 2026 WarcraftXL. GPLv3.

#include "game/Binding.hpp"
#include "offsets/engine/Lua.hpp"
#include "wxl/FrameScriptApi.h"
#include "wxl/NetworkApi.h"
#include "wxl/PluginApi.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace
{
    constexpr uint16_t kRequest = 0x0550;
    constexpr uint16_t kState = 0x0551;
    constexpr size_t kMaxRequest = 64;
    constexpr size_t kMaxState = 8192;
    constexpr size_t kMaxQueued = 256;

    WXL_NetworkApi const* network = nullptr;
    std::mutex queueMutex;
    std::deque<std::string> queued;

    void __cdecl OnState(uint8_t const* bytes, uint32_t size, void*)
    {
        if (!bytes || !size || size > kMaxState ||
            std::any_of(bytes, bytes + size, [](uint8_t value)
            { return (value < 0x20 && value != '\n') || value > 0x7e; }))
            return;
        size_t begin = 0;
        for (size_t end = 0; end <= size; ++end)
        {
            if (end != size && bytes[end] != '\n')
                continue;
            if (end == begin || end - begin > 240)
                return;
            begin = end + 1;
        }
        std::lock_guard lock(queueMutex);
        begin = 0;
        for (size_t end = 0; end <= size; ++end)
        {
            if (end != size && bytes[end] != '\n')
                continue;
            if (queued.size() == kMaxQueued)
                queued.pop_front();
            queued.emplace_back(reinterpret_cast<char const*>(bytes + begin), end - begin);
            begin = end + 1;
        }
    }

    int __cdecl Send(void* state)
    {
        namespace lua = wxl::offsets::engine::lua;
        size_t size = 0;
        char const* text = wxl::game::Native<lua::LuaToStringFn>(lua::kLuaToString)(state, 1, &size);
        bool const valid = text && size && size <= kMaxRequest &&
            std::all_of(text, text + size, [](char value)
            { return value >= 0x20 && value <= 0x7e; });
        bool const sent = valid && network->Send(kRequest,
            reinterpret_cast<uint8_t const*>(text), static_cast<uint32_t>(size)) != 0;
        wxl::game::Native<lua::LuaPushBooleanFn>(lua::kLuaPushBoolean)(state, sent);
        return 1;
    }

    int __cdecl Pop(void* state)
    {
        namespace lua = wxl::offsets::engine::lua;
        std::string message;
        {
            std::lock_guard lock(queueMutex);
            if (queued.empty())
            {
                wxl::game::Native<lua::LuaPushNilFn>(lua::kLuaPushNil)(state);
                return 1;
            }
            message = std::move(queued.front());
            queued.pop_front();
        }
        wxl::game::Native<lua::LuaPushLStringFn>(lua::kLuaPushLString)(
            state, message.data(), message.size());
        return 1;
    }

    int __cdecl Clear(void*)
    {
        std::lock_guard lock(queueMutex);
        queued.clear();
        return 0;
    }
}

const WXL_PluginInfo* __cdecl WXL_Query(void)
{
    static const WXL_PluginInfo info{
        sizeof(WXL_PluginInfo), WXL_API_VERSION, "wxl-area-loot", 1, WXL_CLIENT_BUILD,
    };
    return &info;
}

int __cdecl WXL_Load(WXL_Api const* api)
{
    if (!api || api->apiVersion != WXL_API_VERSION)
        return 0;
    network = static_cast<WXL_NetworkApi const*>(
        api->GetInterface("wxl.network", WXL_NETWORK_API_VERSION));
    auto const* script = static_cast<WXL_FrameScriptApi const*>(
        api->GetInterface("wxl.framescript", WXL_FRAME_SCRIPT_API_VERSION));
    if (!network || !script)
        return 0;
    return network->RegisterClientOpcode(kRequest, "CMSG_WXL_AREA_LOOT_REQUEST") &&
        network->RegisterServerOpcode(kState, "SMSG_WXL_AREA_LOOT_STATE", &OnState, nullptr) &&
        script->RegisterFunction("_WXLWOW_AREA_LOOT_SEND", &Send) &&
        script->RegisterFunction("_WXLWOW_AREA_LOOT_POP", &Pop) &&
        script->RegisterFunction("_WXLWOW_AREA_LOOT_CLEAR", &Clear) &&
        script->RegisterCVar("wxlAreaLootAuto", "1") &&
        script->RegisterCVar("wxlAreaLootQuality", "0");
}
