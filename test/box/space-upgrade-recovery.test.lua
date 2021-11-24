test_run = require('test_run').new()

t1 = box.schema.space.create('test1')
_ = t1:create_index('pk')
t1:format({{"f1", "unsigned"}, {"f2", "unsigned"}})

t2 = box.schema.space.create('test2')
_ = t2:create_index('pk')
t2:format({{"f1", "unsigned"}, {"f2", "unsigned"}})

t3 = box.schema.space.create('test3')
_ = t3:create_index('pk')
t3:format({{"f1", "unsigned"}, {"f2", "unsigned"}})

t4 = box.schema.space.create('test4')
_ = t4:create_index('pk')
t4:format({{"f1", "unsigned"}, {"f2", "unsigned"}})

row_cnt = 20

for i = 1,row_cnt do\
    t1:replace({i, i})\
    t2:replace({i, i})\
    t3:replace({i, i})\
    t4:replace({i, i})\
end
box.error.injection.set('ERRINJ_SPACE_UPGRADE_DELAY', true)

test_run:cmd("setopt delimiter ';'")

body1   = [[
function(tuple)
    local new_tuple = {}
    new_tuple[1] = tuple[1]
    new_tuple[2] = tostring(tuple[2])
    return new_tuple
end
]];

body2   = [[
function(tuple)
    if tuple[1] >= 15 then error("boom") end
    local new_tuple = {}
    new_tuple[1] = tuple[1]
    new_tuple[2] = tostring(tuple[2])
    return new_tuple
end
]];

test_run:cmd("setopt delimiter ''");
box.schema.func.create("upgrade_func", {body = body1, is_deterministic = true, is_sandboxed = true})
box.schema.func.create("error_func", {body = [[function(tuple) error("boom") return end]], is_deterministic = true, is_sandboxed = true})
box.schema.func.create("error_delayed_func", {body = body2, is_deterministic = true, is_sandboxed = true})

new_format = {{name="x", type="unsigned"}, {name="y", type="string"}}

_ = t1:upgrade({mode = "notest", func = "upgrade_func", format = new_format, background = true})
_ = t2:upgrade({mode = "test", func = "upgrade_func", format = new_format, background = true})
_ = t3:upgrade({mode = "notest", func = "error_func", format = new_format, background = true})
_ = t4:upgrade({mode = "notest", func = "error_delayed_func", format = new_format, background = true})

-- Give fibers some time to proceed upgrade.
require('fiber').sleep(0.1)

t = box.space._space_upgrade:select(t1.id)
assert(t[1][2] == "inprogress")
t = box.space._space_upgrade:select(t2.id)
assert(t[1][2] == "test")
t = box.space._space_upgrade:select(t3.id)
assert(t[1][2] == "error")
t = box.space._space_upgrade:select(t4.id)
assert(t[1][2] == "inprogress")

test_run:cmd("restart server default")

row_cnt = 20
t1 = box.space.test1
t2 = box.space.test2
t3 = box.space.test3
t4 = box.space.test4
t = box.space._space_upgrade:select(t1.id)
-- inprogress upgrade should finish.
assert(t[1] == nil)
t = t1.index[0]:get({row_cnt})
assert(t[2] == "20")
-- test upgrade should disappear.
t = box.space._space_upgrade:select(t2.id)
assert(t[1] == nil)
t = t2.index[0]:get({row_cnt})
assert(t[2] == 20)
-- error upgrade should remain unchanged.
t = box.space._space_upgrade:select(t3.id)
assert(t[1][2] == "error")
t3.index[0]:get({1})
-- ugprade change state to error during recovery.
t = box.space._space_upgrade:select(t4.id)
assert(t[1][2] == "error")
t4.index[0]:get({1})
t4.index[0]:get({row_cnt})

new_format = {{name="x", type="unsigned"}, {name="y", type="string"}}
t3:upgrade({mode = "notest", func = "upgrade_func", format = new_format})
t4:upgrade({mode = "notest", func = "upgrade_func", format = new_format})

box.space.test1:drop()
box.space.test2:drop()
box.space.test3:drop()
box.space.test4:drop()
box.schema.func.drop("upgrade_func")
box.schema.func.drop("error_func")
