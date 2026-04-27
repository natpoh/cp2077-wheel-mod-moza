local dw_raw = nil
local dw_game = nil
local dw_enabled = nil

registerForEvent("onInit", function()
    -- Native global functions are exposed on the Game object
    dw_raw = Game["DirectWheel_GetDebugRawSteer"]
    dw_game = Game["DirectWheel_GetDebugWheelSteer"]
    dw_enabled = Game["DirectWheel_IsDebugLoggingEnabled"]
end)

registerForEvent("onDraw", function()
    ImGui.SetNextWindowPos(20, 20, ImGuiCond.FirstUseEver)
    ImGui.SetNextWindowSize(300, 100, ImGuiCond.FirstUseEver)
    
    if ImGui.Begin("Direct Wheel Debug") then
        if dw_enabled == nil then
            dw_enabled = Game["DirectWheel_IsDebugLoggingEnabled"]
            dw_raw = Game["DirectWheel_GetDebugRawSteer"]
            dw_game = Game["DirectWheel_GetDebugWheelSteer"]
        end
        
        if type(dw_enabled) == "function" then
            if dw_enabled() then
                ImGui.Text(string.format("Steering (Wheel): %.1f", dw_raw() * 100.0))
                ImGui.Text(string.format("Steering (Game):  %.1f", dw_game() * 100.0))
            else
                ImGui.Text("Debug logging is disabled in Mod Settings.")
            end
        else
            ImGui.Text("Waiting for RED4ext direct_wheel...")
        end
    end
    ImGui.End()
end)
