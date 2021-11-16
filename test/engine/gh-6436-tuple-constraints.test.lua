-- https://github.com/tarantool/tarantool/issues/6436 Constraints

-- Field constraint basic tests
inspector = require('test_run').new()
engine = inspector:get_cfg('engine')
constr_field_body1 = "function(field) if field >= 100 then error('100!') end return true end" \
constr_field_body2 = "function(field) if field >= 200 then error('200!') end return true end" \
constr_field_body3 = "function(field) if field >= 300 then error('300!') end return true end"
space_format = { {"id1", constraint='field_constr1'},                  \
                 {"id2", constraint={'field_constr2'}},                \
                 {"id3", constraint={my_constr = 'field_constr3'}} }

box.schema.func.create('field_constr1', {language = 'LUA', is_deterministic = true, body = constr_field_body1})
box.schema.func.create('field_constr2', {language = 'LUA', is_deterministic = true, body = constr_field_body2})
box.schema.func.create('field_constr3', {language = 'LUA', is_deterministic = true, body = constr_field_body3})

s = box.schema.create_space('test', {engine=engine, format = space_format})
_ = s:create_index('pk')
s:replace{1, 2, 3} -- ok
s:replace{100, 2, 3} -- fail
s:replace{1, 200, 3} -- fail
s:replace{1, 2, 300} -- fail

require('test_run').new():cmd("restart server default")
s = box.space.test
s:replace{1, 2, 3} -- ok
s:replace{100, 2, 3} -- fail
s:replace{1, 200, 3} -- fail
s:replace{1, 2, 300} -- fail

box.snapshot()
require('test_run').new():cmd("restart server default")
s = box.space.test
s:replace{1, 2, 3} -- ok
s:replace{100, 2, 3} -- fail
s:replace{1, 200, 3} -- fail
s:replace{1, 2, 300} -- fail

_ = s:delete{1}
_ = s:create_index('sk', {unique=false, parts={{'[4].field', 'unsigned'}}})
s:replace{1, 2, 3} -- fail
s:replace{1, 2, 3, {field=0}} -- ok
s:replace{100, 2, 3, {field=0}} -- fail
s:replace{1, 200, 3, {field=0}} -- fail
s:replace{1, 2, 300, {field=0}} -- fail

box.snapshot()
require('test_run').new():cmd("restart server default")
s = box.space.test
s:replace{1, 2, 3, {field=0}} -- ok
s:replace{100, 2, 3, {field=0}} -- fail
s:replace{1, 200, 3, {field=0}} -- fail
s:replace{1, 2, 300, {field=0}} -- fail

s:drop()
box.func.field_constr1:drop()
box.func.field_constr2:drop()
box.func.field_constr3:drop()

-- Wrong constraints
inspector = require('test_run').new()
engine = inspector:get_cfg('engine')
constr_field_body1 = "function(field) if field >= 100 then error('100!') end return true end" \
constr_field_body2 = "function(field) if field >= 200 then error('200!') end return true end" \
function field_constr3(x) end
box.schema.func.create('field_constr1', {language = 'LUA', is_deterministic = true, body = constr_field_body1})
box.schema.func.create('field_constr2', {language = 'LUA', body = constr_field_body2})
box.schema.func.create('field_constr3', {language = 'LUA', is_deterministic = true})

space_format = {{"id1", constraint = 'field_constr1'}, {"id2"}}
s = box.schema.create_space('test', {engine=engine, format = space_format})
_ = s:create_index('pk')

s:format({{"id1", constraint = "field_constr4"}, {"id2"}})
s:format({{"id1", constraint = {"field_constr4"}}, {"id2"}})
s:format({{"id1", constraint = {field_constr1="field_constr4"}}, {"id2"}})
s:format({{"id1", constraint = 666}, {"id2"}})
s:format({{"id1", constraint = {666}}, {"id2"}})
s:format({{"id1", constraint = {666}}, {"id2"}})
s:format({{"id1", constraint = "field_constr2"}, {"id2"}})
s:format({{"id1", constraint = "field_constr3"}, {"id2"}})
s:format({{"id1", constraint = {"field_constr2"}}, {"id2"}})
s:format({{"id1", constraint = {"field_constr3"}}, {"id2"}})

s:format()

s:drop()
box.func.field_constr1:drop()
box.func.field_constr2:drop()
box.func.field_constr3:drop()

-- Several constraints in one field
inspector = require('test_run').new()
engine = inspector:get_cfg('engine')
constr_field_body1 = "function(field) if field < 0 then error('<0') end return true end" \
constr_field_body2 = "function(field) if field >= 100 then error('>=100!') end return true end" \
box.schema.func.create('ge_zero', {language = 'LUA', is_deterministic = true, body = constr_field_body1})
box.schema.func.create('lt_100', {language = 'LUA', is_deterministic = true, body = constr_field_body2})
space_format = { {"id1"},                                               \
                 {"id2", constraint={'ge_zero', 'lt_100'}} }
s = box.schema.create_space('test', {engine=engine, format = space_format})
_ = s:create_index('pk')

s:replace{1, 0}   -- ok
s:replace{2, -1}  -- fail
s:replace{3, 100} -- fail

require('test_run').new():cmd("restart server default")
s = box.space.test
s:replace{1, 0}   -- ok
s:replace{2, -1}  -- fail
s:replace{3, 100} -- fail

s:drop()
box.func.ge_zero:drop()
box.func.lt_100:drop()

-- Field constraint integrity tests
inspector = require('test_run').new()
engine = inspector:get_cfg('engine')
constr_field_body1 = "function(field) if field >= 100 then error('100!') end return true end" \
constr_field_body2 = "function(field) if field >= 200 then error('200!') end return true end" \
constr_field_body3 = "function(field) if field >= 300 then error('300!') end return true end"
box.schema.func.create('field_constr1', {language = 'LUA', is_deterministic = true, body = constr_field_body1})
box.schema.func.create('field_constr2', {language = 'LUA', is_deterministic = true, body = constr_field_body2})
box.schema.func.create('field_constr3', {language = 'LUA', is_deterministic = true, body = constr_field_body3})
s = box.schema.create_space('test', {engine=engine})
_ = s:create_index('pk')

s:format()
s:format{{"id1", constraint='unknown_constr'}} -- fail, func not found
s:format()
s:format{{"id1", constraint='field_constr1'}} -- ok
s:replace{1, 2, 3}
s:replace{100, 2, 3} -- fail
box.func.field_constr1:drop() -- fail, is used

require('test_run').new():cmd("restart server default")
box.func.field_constr1:drop() -- fail, is used
box.snapshot()
require('test_run').new():cmd("restart server default")
box.func.field_constr1:drop() -- fail, is used
s = box.space.test

s:replace{100, 2, 3} -- still fail
s:format{{"id1"}, {"id2", constraint='field_constr2'}}
box.func.field_constr1:drop() -- ok now
s:replace{100, 2, 3} -- ok now
s:replace{1, 200, 3} -- fail
s:replace{1, 2, 300} -- ok

s:select{}
s:format()
s:format{{"id1"}, {"id2", constraint='field_constr2'}, {"id3", constraint='field_constr3'}} -- fail
s:replace{1, 200, 3} -- fail
s:select{}
s:format()

s:drop()
box.func.field_constr1
box.func.field_constr2:drop()
box.func.field_constr3:drop()

-- Check that sql and net.box works with format correctly.
inspector = require('test_run').new()
engine = inspector:get_cfg('engine')
constr_field_body = "function(field) if field >= 100 then error('100!') end return true end"
box.schema.func.create('field_constr', {language = 'LUA', is_deterministic = true, body = constr_field_body})
s = box.schema.create_space('test', {engine=engine})
s:format{{'id1', constraint='field_constr'},{'id2'},{'id3'}}
_ = s:create_index('pk')

s:replace{1, 2, 3}
s:replace{100, 2, 3}
s:get{1}.id1
s:get{1}.id2
s:get{1}.id3

box.execute[[SELECT * FROM "test"]]

box.schema.user.grant('guest', 'read,write', 'space', 'test', nil, {if_not_exists=true})
conn = require('net.box').connect(box.cfg.listen)
conn.space.test:select{}
conn.space.test:get{1}.id1
conn.space.test:get{1}.id2
conn.space.test:get{1}.id3

conn:execute[[SELECT * FROM "test"]]

s:drop()
box.func.field_constr:drop()
