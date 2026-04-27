local scanned = false
local file = nil

local function writeLine(text)
    if file then
        file:write(text .. "\n")
        file:flush()
    end
end

local function try(obj, name)
    if not obj then return end

    local ok, result = pcall(function()
        return obj[name](obj)
    end)

    if ok and result ~= nil then
        local out = tostring(result)

        -- если это RecordID
        if type(result) == "userdata" and result.value then
            out = result.value
        end

        writeLine("[OK] " .. name .. " = " .. out)
    else
        writeLine("[NO] " .. name)
    end
end

registerForEvent("onUpdate", function()
    if scanned then return end

    local player = Game.GetPlayer()
    if not player then return end

    local vehicle = Game.GetMountedVehicle(player)
    if not vehicle then return end

    scanned = true

    file = io.open("vehicle_scan.txt", "w")

    if not file then
        print("НЕ удалось создать файл!")
        return
    end

    writeLine("==== VEHICLE SCAN START ====")

    local methods = {
        "GetRecordID",
        "GetRecord",
        "GetWorldPosition",
        "GetWorldForward",
        "GetWorldTransform",
        "GetVelocity",
        "GetSpeed",
        "GetCurrentSpeed",
        "GetKmphSpeed",
        "GetVehiclePS",
        "GetGame",
        "GetEntityID",
        "IsPlayerMounted",
        "GetDisplayName",
        "GetName"
    }

    for _, m in ipairs(methods) do
        try(vehicle, m)
    end

    local ok, record = pcall(function()
        return vehicle:GetRecord()
    end)

    if ok and record then
        writeLine("---- RECORD ----")

        local recordMethods = {
            "GetID",
            "DisplayName",
            "Manufacturer",
            "DriveModelData",
            "Mass",
            "maxWheelTurnDeg",
            "wheelTurnMaxAddPerSecond",
            "wheelTurnMaxSubPerSecond",
            "Tags"
        }

        for _, m in ipairs(recordMethods) do
            try(record, m)
        end
    else
        writeLine("NO RECORD")
    end

    writeLine("==== VEHICLE SCAN END ====")

    file:close()
    file = nil

    print("=== СКАН ЗАВЕРШЁН → vehicle_scan.txt ===")
end)

print("=== Scanner with file logging loaded ===")