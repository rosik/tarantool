test_run = require('test_run').new()

test_run:cmd('create server master with script="replication/master1.lua"')
test_run:cmd('create server rw with rpl_master=master,\
                                         script="replication/replica1.lua"')
test_run:cmd('create server ro with rpl_master=master,\
                                         script="replication/replica2.lua"')

test_run:cmd('start server master')
test_run:switch('master')
box.schema.user.grant('guest', 'replication')
box.cfg{replication_timeout=1}
t = box.schema.space.create('test')
_ = box.space.test:create_index('pk')
t:format({{"f1", "unsigned"}, {"f2", "unsigned"}})

test_run:cmd('start server rw with args="1"')
test_run:cmd('start server ro with args="1"')
test_run:switch('ro')
box.cfg{read_only=true}
test_run:switch('master')
row_cnt = 20

for i = 1,row_cnt do\
    box.space.test:replace({i, i})\
end
box.error.injection.set('ERRINJ_SPACE_UPGRADE_DELAY', true)

test_run:cmd("setopt delimiter ';'")
body   = [[
function(tuple)
    local new_tuple = {}
    new_tuple[1] = tuple[1]
    new_tuple[2] = tostring(tuple[2])
    return new_tuple
end
]];
test_run:cmd("setopt delimiter ''");
box.schema.func.create("upgrade_func", {body = body, is_deterministic = true, is_sandboxed = true})
new_format = {{name="x", type="unsigned"}, {name="y", type="string"}}

f = box.space.test:upgrade({mode = "notest", func = "upgrade_func", format = new_format, background = true})
t = box.space._space_upgrade:select()
assert(t[1][2] == "inprogress")

test_run:switch('rw')
t = box.space._space_upgrade:select()
assert(t[1][2] == "inprogress")
assert(box.cfg.read_only == true)
t = box.space.test.index[0]:get({15})
assert(t[2] == "15")
t = box.space.test.index[0]:get({5})
assert(t[2] == "5")
-- Should fail due to read_only status.
box.space.test:replace({100, 100})
box.space.test:replace({100, 'asd'})

test_run:switch('ro')
t = box.space._space_upgrade:select()
assert(t[1][2] == "inprogress")
assert(box.cfg.read_only == true)
t = box.space.test.index[0]:get({15})
assert(t[2] == "15")
t = box.space.test.index[0]:get({5})
assert(t[2] == "5")

test_run:switch('master')
box.error.injection.set('ERRINJ_SPACE_UPGRADE_DELAY', false)
f:set_joinable(true)
f:join()

t = box.space._space_upgrade:select()
assert(t[1] == nil)

test_run:switch('rw')
t = box.space.test.index[0]:get({20})
assert(t[2] == "20")

assert(box.cfg.read_only == false)
box.space.test:replace({100, 100})
box.space.test:replace({100, 'asd'})
t = box.space._space_upgrade:select()
assert(t[1] == nil)

test_run:switch('ro')
assert(box.cfg.read_only == true)
t = box.space._space_upgrade:select()
assert(t[1] == nil)

test_run:switch('master')

box.space.test:drop()
box.schema.func.drop("upgrade_func")

-- Cleanup.
test_run:switch('default')
test_run:cmd('stop server ro')
test_run:cmd('stop server rw')
test_run:cmd('stop server master')
test_run:cmd('delete server rw')
test_run:cmd('delete server ro')
test_run:cmd('delete server master')

