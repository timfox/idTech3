-- Simple patrol AI sample for scripted entities
-- Usage: bind these callbacks to your entity in your scripting layer

local PatrolAI = {}
PatrolAI.waypoints = {
    { x = 100, y = 200, z = 0 },
    { x = 300, y = 200, z = 0 },
    { x = 300, y = 400, z = 0 },
    { x = 100, y = 400, z = 0 },
}
PatrolAI.current = 1
PatrolAI.pauseTime = 1000 -- ms to wait at each point
PatrolAI.lastSwitch = 0

-- Called each frame; dt is milliseconds since last tick
function PatrolAI:update(dt, selfEntity)
    if not selfEntity or #self.waypoints == 0 then
        return
    end

    local now = selfEntity:time()
    local target = self.waypoints[self.current]
    local dist = selfEntity:distance2d(target.x, target.y)

    -- If close enough, pause then switch to next
    if dist < 16 then
        if self.lastSwitch == 0 then
            self.lastSwitch = now
            selfEntity:say("Standing by...")
            return
        end
        if now - self.lastSwitch >= self.pauseTime then
            self.current = (self.current % #self.waypoints) + 1
            self.lastSwitch = 0
            selfEntity:say("Moving to waypoint " .. self.current)
        end
        return
    end

    -- Move toward target
    selfEntity:moveTo(target.x, target.y, target.z or 0, 120) -- speed units/sec
end

return PatrolAI

