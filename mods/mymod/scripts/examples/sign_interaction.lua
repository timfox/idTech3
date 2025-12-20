-- Interactive sign sample (Zelda-style)
-- Hook this to a use event on a map entity; it will center-print the text.

local Sign = {}

Sign.text = "Beware of the cave ahead!"

-- Called when player uses/activates the sign
function Sign:onUse(user)
    if not user then return end
    -- Center print to the activating player only
    user:centerPrint(self.text)
end

return Sign

