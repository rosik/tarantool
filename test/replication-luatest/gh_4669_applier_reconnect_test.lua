local t = require('luatest')
local cluster = require('test.luatest_helpers.cluster')
local asserts = require('test.luatest_helpers.asserts')
local helpers = require('test.luatest_helpers')

local g = t.group('gh-4669-applier-reconnect')

g.before_each(function()
    g.cluster = cluster:new({})
    g.master = g.cluster:build_server({alias = 'master'})
    local box_cfg = {
        replication = ('%s/master.iproto'):format(helpers.SOCKET_DIR),
    }
    g.replica = g.cluster:build_server({alias = 'replica', box_cfg = box_cfg})
    g.replica2 = g.cluster:build_server({alias = 'replica2', box_cfg = box_cfg})

    g.cluster:add_server(g.master)
    g.cluster:add_server(g.replica)
    g.cluster:add_server(g.replica2)
    g.cluster:start()
    asserts:assert_server_follow_upstream(g.replica, 1)
end)

g.after_each(function()
    g.cluster:stop()
end)

-- Test that appliers aren't recreated upon replication reconfiguration.
-- Add and then remove two extra replicas to the configuration. The master
-- connection should stay intact.
g.test_applier_connection_on_reconfig = function(g)
    g.replica:eval(([[
        box.cfg{
            replication = {
                box.cfg.listen,
                box.cfg.replication[1],
                "%s/replica2.iproto",
            }
        }]]):format(helpers.SOCKET_DIR))
    g.replica:eval([[
        box.cfg{
            replication = {
                box.cfg.replication[2]
            }
        }
    ]])
    asserts:assert_server_follow_upstream(g.replica, 1)
    t.assert_equals(g.master:grep_log("exiting the relay loop"), nil)
end
