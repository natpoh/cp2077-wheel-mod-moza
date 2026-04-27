-- Direct Wheel — Speed-Sensitive Steering (reads settings from config.json)
-- No UI overlay — all tuning is done via Mod Settings in-game.
-- This script reads config.json written by the RED4ext plugin and applies
-- TweakDB overrides for maxWheelTurnDeg, wheelTurnMaxAddPerSecond,
-- wheelTurnMaxSubPerSecond each frame while driving.

local configPath = "red4ext/plugins/direct_wheel/config.json"

local mod = {
    enabled = true,
    -- Values read from config.json (defaults match Mod Settings defaults)
    speedFactor    = 0.015,
    minTurn        = 12,
    addBoostFactor = 0.008,
    subBoostFactor = 0.012,

    -- Internal state
    origTurn = nil,
    origAdd  = nil,
    origSub  = nil,
    lastDriveModel = nil,
    lastRoundedTurn = nil,
    configReloadTimer = 0,
}

-- Simple JSON value extraction (no external deps)
local function jsonInt(text, key)
    local pat = '"' .. key .. '"%s*:%s*(%d+)'
    local v = text:match(pat)
    return v and tonumber(v) or nil
end

local function reloadConfig()
    local f = io.open(configPath, "r")
    if not f then return end
    local text = f:read("*a")
    f:close()

    local sf = jsonInt(text, "steeringSpeedFactor")
    local mt = jsonInt(text, "steeringMinTurn")
    local ab = jsonInt(text, "steeringAddBoost")
    local sb = jsonInt(text, "steeringSubBoost")

    if sf then mod.speedFactor    = sf / 1000.0 end
    if mt then mod.minTurn        = mt end
    if ab then mod.addBoostFactor = ab / 1000.0 end
    if sb then mod.subBoostFactor = sb / 1000.0 end
end

local function getSpeed(vehicle)
    local ok, speed = pcall(function() return vehicle:GetCurrentSpeed() end)
    if ok and speed and type(speed) == "number" then return speed * 3.6 end
    return 0
end

local function getDriveModelPath(vehicle)
    local recordID = vehicle:GetRecordID()
    if not recordID then return nil end
    local ok, ref = pcall(function()
        return TweakDB:GetFlat(tostring(recordID) .. ".vehDriveModelData")
    end)
    if ok and ref then return tostring(ref) end
    return nil
end

-- Load config on startup
reloadConfig()

registerForEvent("onUpdate", function(deltaTime)
    if not mod.enabled then return end

    -- Reload config every 2 seconds (picks up slider changes)
    mod.configReloadTimer = mod.configReloadTimer + deltaTime
    if mod.configReloadTimer > 2.0 then
        mod.configReloadTimer = 0
        reloadConfig()
    end

    local player = Game.GetPlayer()
    if not player then return end
    local vehicle = Game.GetMountedVehicle(player)

    if not vehicle then
        if mod.lastDriveModel and mod.origTurn then
            pcall(function()
                TweakDB:SetFlat(mod.lastDriveModel .. ".maxWheelTurnDeg",          mod.origTurn)
                TweakDB:SetFlat(mod.lastDriveModel .. ".wheelTurnMaxAddPerSecond", mod.origAdd)
                TweakDB:SetFlat(mod.lastDriveModel .. ".wheelTurnMaxSubPerSecond", mod.origSub)
                TweakDB:Update(mod.lastDriveModel)
            end)
        end
        mod.origTurn = nil
        mod.origAdd  = nil
        mod.origSub  = nil
        mod.lastDriveModel = nil
        mod.lastRoundedTurn = nil
        return
    end

    local driveModelRecord = getDriveModelPath(vehicle)
    if not driveModelRecord then return end

    -- Cache stock values ONCE per vehicle
    if driveModelRecord ~= mod.lastDriveModel then
        local ok1, t = pcall(function() return TweakDB:GetFlat(driveModelRecord .. ".maxWheelTurnDeg") end)
        local ok2, a = pcall(function() return TweakDB:GetFlat(driveModelRecord .. ".wheelTurnMaxAddPerSecond") end)
        local ok3, s = pcall(function() return TweakDB:GetFlat(driveModelRecord .. ".wheelTurnMaxSubPerSecond") end)
        mod.origTurn = (ok1 and t) or 40
        mod.origAdd  = (ok2 and a) or 100
        mod.origSub  = (ok3 and s) or 140
        mod.lastDriveModel = driveModelRecord
        mod.lastRoundedTurn = nil
        print(string.format("[steering] mounted: %s turn=%.1f add=%.0f sub=%.0f",
              driveModelRecord, mod.origTurn, mod.origAdd, mod.origSub))
    end

    local speed = getSpeed(vehicle)

    local turn = mod.origTurn / (1 + speed * mod.speedFactor)
    if turn < mod.minTurn then turn = mod.minTurn end
    local turnAdd = mod.origAdd * (1 + speed * mod.addBoostFactor)
    local turnSub = mod.origSub * (1 + speed * mod.subBoostFactor)

    local rounded = math.floor(turn * 10 + 0.5) / 10
    if rounded ~= mod.lastRoundedTurn then
        pcall(function()
            TweakDB:SetFlat(driveModelRecord .. ".maxWheelTurnDeg",          turn)
            TweakDB:SetFlat(driveModelRecord .. ".wheelTurnMaxAddPerSecond", turnAdd)
            TweakDB:SetFlat(driveModelRecord .. ".wheelTurnMaxSubPerSecond", turnSub)
            TweakDB:Update(driveModelRecord)
        end)
        mod.lastRoundedTurn = rounded
    end
end)

print("=== [steering] background loaded (reads config.json) ===")
