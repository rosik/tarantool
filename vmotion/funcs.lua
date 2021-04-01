-- setup needed code routines
function spec_insert(space, id)
    if space:count() == 0 then
        space:insert({id, id, id})
        return
    end
    local le = space:select(id, {iterator='LE'})
    if #le == 0 then
        le = space.index[0]:max()
    else
        le = le[1]
    end
    local l = le[1]
    local ge = space:select(id, {iterator='GE'})
    if #ge == 0 then
        ge = space.index[0]:min()
    else
        ge = ge[1]
    end
    local g = ge[1]
    if l == id or g == id then
        print('Error')  
        return
    end
    space:update(g, {{'=', 2, id}})
    space:update(l, {{'=', 3, id}})
    space:insert({id, l, g})
    return
end
function spec_delete(space, id)
    if space:get(id) == nil then
        return
    end
    space:delete(id)
    if space:count() == 0 then
        return
    end
    local le = space:select(id, {iterator='LE'})
    if #le == 0 then
        le = space.index[0]:max()
    else
        le = le[1]
    end
    local l = le[1]
    local ge = space:select(id, {iterator='GE'})
    if #ge == 0 then
        ge = space.index[0]:min()
    else
        ge = ge[1]
    end
    local g = ge[1]
    space:update(g, {{'=', 2, l}})
    space:update(l, {{'=', 3, g}})
    return
end
