-- Direct Wheel — Speed-Sensitive Steering (CET companion)
-- Modifies TweakDB driveModelData per-vehicle to compensate for the game's
-- exponential steering reduction at speed.
--
-- Three parameters are adjusted:
--   maxWheelTurnDeg          — max wheel turn angle (degrees)
--   wheelTurnMaxAddPerSecond — how fast wheels turn toward steer input
--   wheelTurnMaxSubPerSecond — how fast wheels return to center
--
-- At rest: stock values. At speed: turn angle decreases but Add/Sub rates
-- increase proportionally so the effective steering stays consistent.

local mod = {
    enabled = true,
    -- Tuning knobs (exposed in ImGui overlay)
    speedFactor    = 0.012,   -- how aggressively maxTurnDeg decreases with speed
    minTurnDeg     = 12.0,    -- floor for maxWheelTurnDeg
    addBoostFactor = 0.008,   -- wheelTurnMaxAddPerSecond boost per km/h
    subBoostFactor = 0.010,   -- wheelTurnMaxSubPerSecond boost per km/h

    -- Per-vehicle cache (reset on dismount)
    origTurn = nil,
    origAdd  = nil,
    origSub  = nil,
    lastDriveModel = nil,
    lastTurn = nil,

    -- Debug HUD data
    debug = nil,
}

-- ── helpers ─────────────────────────────────────────────────────────────
local function getSpeed(vehicle)
    local ok, speed = pcall(function() return vehicle:GetCurrentSpeed() end)
    if ok and speed and type(speed) == "number" then return speed * 3.6 end
    return 0
end

local function getDriveModelPath(vehicle)
    local recordID = vehicle:GetRecordID()
    if not recordID then return nil end
    local vehStr = tostring(recordID)
    local ok, ref = pcall(function()
        return TweakDB:GetFlat(vehStr .. ".vehDriveModelData")
    end)
    if ok and ref then return tostring(ref) end
    return nil
end

local function readFlat(path)
    local ok, val = pcall(function() return TweakDB:GetFlat(path) end)
    if ok and val then return val end
    return nil
end

local function writeFlat(path, val)
    local ok, err = pcall(function() TweakDB:SetFlat(path, val) end)
    if not ok then
        print("[direct_wheel_steering] SetFlat FAILED:", path, err)
    end
end

-- ── main loop ───────────────────────────────────────────────────────────
registerForEvent("onUpdate", function(deltaTime)
    if not mod.enabled then return end

    local player = Game.GetPlayer()
    if not player then return end
    local vehicle = Game.GetMountedVehicle(player)

    if not vehicle then
        -- Dismounted: restore originals if we have them
        if mod.lastDriveModel and mod.origTurn then
            writeFlat(mod.lastDriveModel .. ".maxWheelTurnDeg",          mod.origTurn)
            writeFlat(mod.lastDriveModel .. ".wheelTurnMaxAddPerSecond", mod.origAdd)
            writeFlat(mod.lastDriveModel .. ".wheelTurnMaxSubPerSecond", mod.origSub)
            print("[direct_wheel_steering] restored originals for", mod.lastDriveModel)
        end
        mod.origTurn = nil
        mod.origAdd  = nil
        mod.origSub  = nil
        mod.lastDriveModel = nil
        mod.lastTurn = nil
        mod.debug = nil
        return
    end

    local driveModel = getDriveModelPath(vehicle)
    if not driveModel then return end

    -- Cache stock values on first encounter
    if driveModel ~= mod.lastDriveModel then
        mod.origTurn = readFlat(driveModel .. ".maxWheelTurnDeg")        or 40
        mod.origAdd  = readFlat(driveModel .. ".wheelTurnMaxAddPerSecond") or 100
        mod.origSub  = readFlat(driveModel .. ".wheelTurnMaxSubPerSecond") or 140
        mod.lastDriveModel = driveModel
        mod.lastTurn = nil
        print(string.format("[direct_wheel_steering] new vehicle: %s  turn=%.1f add=%.0f sub=%.0f",
              driveModel, mod.origTurn, mod.origAdd, mod.origSub))
    end

    local speed = getSpeed(vehicle)

    -- ── Compute adjusted values ─────────────────────────────────────────
    -- Turn angle decreases with speed (fight the game's built-in reduction)
    local turn = mod.origTurn / (1.0 + speed * mod.speedFactor)
    if turn < mod.minTurnDeg then turn = mod.minTurnDeg end

    -- Add/Sub rates increase with speed (compensate game's sluggish response)
    local addRate = mod.origAdd * (1.0 + speed * mod.addBoostFactor)
    local subRate = mod.origSub * (1.0 + speed * mod.subBoostFactor)

    -- Only write if values actually changed (avoid spamming TweakDB)
    local roundedTurn = math.floor(turn * 10 + 0.5) / 10
    if roundedTurn ~= mod.lastTurn then
        writeFlat(driveModel .. ".maxWheelTurnDeg",          turn)
        writeFlat(driveModel .. ".wheelTurnMaxAddPerSecond", addRate)
        writeFlat(driveModel .. ".wheelTurnMaxSubPerSecond", subRate)
        mod.lastTurn = roundedTurn
    end

    mod.debug = {
        driveModel = driveModel,
        speed      = speed,
        origTurn   = mod.origTurn,
        origAdd    = mod.origAdd,
        origSub    = mod.origSub,
        turn       = turn,
        addRate    = addRate,
        subRate    = subRate,
    }
end)

-- ── ImGui debug overlay ─────────────────────────────────────────────────
registerForEvent("onDraw", function()
    if not mod.enabled then return end

    ImGui.Begin("Steering Tweaks", ImGuiWindowFlags.AlwaysAutoResize)

    if mod.debug then
        local d = mod.debug
        ImGui.Text("DriveModel: " .. d.driveModel)
        ImGui.Separator()
        ImGui.Text(string.format("Speed:    %.0f km/h", d.speed))
        ImGui.Text(string.format("TurnDeg:  %.1f  (stock %.1f)", d.turn, d.origTurn))
        ImGui.Text(string.format("AddRate:  %.0f  (stock %.0f)", d.addRate, d.origAdd))
        ImGui.Text(string.format("SubRate:  %.0f  (stock %.0f)", d.subRate, d.origSub))
    else
        ImGui.TextColored(1, 0.3, 0.3, 1, "Not in a vehicle")
    end

    ImGui.Separator()
    local changed
    changed, mod.speedFactor    = ImGui.SliderFloat("Turn speed factor",  mod.speedFactor,    0.001, 0.03)
    changed, mod.minTurnDeg     = ImGui.SliderFloat("Min turn deg",       mod.minTurnDeg,     3.0,   30.0)
    changed, mod.addBoostFactor = ImGui.SliderFloat("Add boost / km/h",   mod.addBoostFactor, 0.001, 0.02)
    changed, mod.subBoostFactor = ImGui.SliderFloat("Sub boost / km/h",   mod.subBoostFactor, 0.001, 0.02)

    if ImGui.Button("Reset to defaults") then
        mod.speedFactor    = 0.012
        mod.minTurnDeg     = 12.0
        mod.addBoostFactor = 0.008
        mod.subBoostFactor = 0.010
    end

    ImGui.End()
end)

print("=== [direct_wheel_steering] CET companion loaded ===")
