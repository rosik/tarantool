local t = require('luatest')
local net = require('net.box')
local cluster = require('test.luatest_helpers.cluster')
local helpers = require('test.luatest_helpers')

local g = t.group('gh_6260')

g.before_test('test_box_status_event', function(cg)
    cg.cluster = cluster:new({})

    local box_cfg = {
        read_only = false
    }

    cg.master = cg.cluster:build_server({alias = 'master', box_cfg = box_cfg})
    cg.cluster:add_server(cg.master)
    cg.cluster:start()
end)


g.after_test('test_box_status_event', function(cg)
    cg.cluster.servers = nil
    cg.cluster:drop()
end)


g.test_box_status_event= function(cg)
    local c = net.connect(cg.master.net_box_uri)

    c:eval([[
        i = ''
        box.watch('box.status', function(...) i = {...} end)
    ]])

    t.assert_equals(c:eval("return i"),
        {"box.status",
            {is_ro = cg.master:eval("return box.info.ro"),
            is_ro_cfg = cg.master:eval("return box.cfg.read_only"),
            status = cg.master:eval("return box.info.status")}})

    -- next step
    cg.master:eval(([[
        box.cfg{
            replication = {
                "%s",
                "%s"
            },
            replication_connect_timeout = 0.001,
        }
    ]]):format(helpers.instance_uri('master'),
               helpers.instance_uri('replica')))

    t.assert_equals(c:eval("return i"),
        {"box.status",
            {is_ro = cg.master:eval("return box.info.ro"),
            is_ro_cfg = cg.master:eval("return box.cfg.read_only"),
            status = cg.master:eval("return box.info.status")}})

    -- next step
    cg.master:eval(([[
        box.cfg{
            replication = {
                "%s",
            },
        }
    ]]):format(helpers.instance_uri('master')))

    t.assert_equals(c:eval("return i"),
        {"box.status",
            {is_ro = cg.master:eval("return box.info.ro"),
            is_ro_cfg = cg.master:eval("return box.cfg.read_only"),
            status = cg.master:eval("return box.info.status")}})

    -- next step
    cg.master:eval("box.cfg{read_only = true}")

    t.assert_equals(c:eval("return i"),
        {"box.status",
            {is_ro = cg.master:eval("return box.info.ro"),
            is_ro_cfg = cg.master:eval("return box.cfg.read_only"),
            status = cg.master:eval("return box.info.status")}})


    c:close()
end


g.before_test('test_box_election_event', function(cg)

    cg.cluster = cluster:new({})

    local box_cfg = {
        replication = {
                        helpers.instance_uri('instance_', 1);
                        helpers.instance_uri('instance_', 2);
                        helpers.instance_uri('instance_', 3);
        },
        replication_connect_quorum = 0,
        election_mode = 'candidate',
        replication_synchro_quorum = 2,
        replication_synchro_timeout = 1,
        replication_timeout = 0.25,
        election_timeout = 0.25,
    }

    cg.instance_1 = cg.cluster:build_server(
        {alias = 'instance_1', box_cfg = box_cfg})

    cg.instance_2 = cg.cluster:build_server(
        {alias = 'instance_2', box_cfg = box_cfg})

    cg.instance_3 = cg.cluster:build_server(
        {alias = 'instance_3', box_cfg = box_cfg})

    cg.cluster:add_server(cg.instance_1)
    cg.cluster:add_server(cg.instance_2)
    cg.cluster:add_server(cg.instance_3)
    cg.cluster:start({cg.instance_1, cg.instance_2, cg.instance_3})
end)


g.after_test('test_box_election_event', function(cg)
    cg.cluster.servers = nil
    cg.cluster:drop({cg.instance_1, cg.instance_2, cg.instance_3})
end)


g.test_box_election_event= function(cg)
    local c1 = net.connect(cg.instance_1.net_box_uri)
    local c2 = net.connect(cg.instance_2.net_box_uri)
    local c3 = net.connect(cg.instance_3.net_box_uri)

    c1:eval([[
        i1 = ''
        box.watch('box.election', function(...) i1 = {...} end)
    ]])
    c2:eval([[
        i2 = ''
        box.watch('box.election', function(...) i2 = {...} end)
    ]])
    c3:eval([[
        i3 = ''
        box.watch('box.election', function(...) i3 = {...} end)
    ]])

    t.assert_equals(c1:eval("return i1"),
        {"box.election",
            {term = cg.instance_1:eval("return box.info.election.term"),
            leader = cg.instance_1:eval("return box.info.election.leader"),
            is_ro = cg.instance_1:eval("return box.info.ro"),
            role = cg.instance_1:eval("return box.info.election.state")}})

    t.assert_equals(c2:eval("return i2"),
        {"box.election",
            {term = cg.instance_2:eval("return box.info.election.term"),
            leader = cg.instance_2:eval("return box.info.election.leader"),
            is_ro = cg.instance_2:eval("return box.info.ro"),
            role = cg.instance_2:eval("return box.info.election.state")}})

    t.assert_equals(c3:eval("return i3"),
        {"box.election",
            {term = cg.instance_3:eval("return box.info.election.term"),
            leader = cg.instance_3:eval("return box.info.election.leader"),
            is_ro = cg.instance_3:eval("return box.info.ro"),
            role = cg.instance_3:eval("return box.info.election.state")}})


    -- next step
    cg.instance_1:eval("return box.cfg{election_mode='voter'}")

    cg.instance_1:wait_election_leader_found()
    cg.instance_2:wait_election_leader_found()
    cg.instance_3:wait_election_leader_found()

    t.assert_equals(c1:eval("return i1"),
        {"box.election",
            {term = cg.instance_1:eval("return box.info.election.term"),
            leader = cg.instance_1:eval("return box.info.election.leader"),
            is_ro = cg.instance_1:eval("return box.info.ro"),
            role = cg.instance_1:eval("return box.info.election.state")}})

    t.assert_equals(c2:eval("return i2"),
        {"box.election",
            {term = cg.instance_2:eval("return box.info.election.term"),
            leader = cg.instance_2:eval("return box.info.election.leader"),
            is_ro = cg.instance_2:eval("return box.info.ro"),
            role = cg.instance_2:eval("return box.info.election.state")}})

    t.assert_equals(c3:eval("return i3"),
        {"box.election",
            {term = cg.instance_3:eval("return box.info.election.term"),
            leader = cg.instance_3:eval("return box.info.election.leader"),
            is_ro = cg.instance_3:eval("return box.info.ro"),
            role = cg.instance_3:eval("return box.info.election.state")}})


    -- next step
    cg.instance_1:eval("return box.cfg{election_mode='candidate'}")
    cg.instance_2:eval("return box.cfg{election_mode='voter'}")

    cg.instance_1:wait_election_leader_found()
    cg.instance_2:wait_election_leader_found()
    cg.instance_3:wait_election_leader_found()

    t.assert_equals(c1:eval("return i1"),
        {"box.election",
            {term = cg.instance_1:eval("return box.info.election.term"),
            leader = cg.instance_1:eval("return box.info.election.leader"),
            is_ro = cg.instance_1:eval("return box.info.ro"),
            role = cg.instance_1:eval("return box.info.election.state")}})

    t.assert_equals(c2:eval("return i2"),
        {"box.election",
            {term = cg.instance_2:eval("return box.info.election.term"),
            leader = cg.instance_2:eval("return box.info.election.leader"),
            is_ro = cg.instance_2:eval("return box.info.ro"),
            role = cg.instance_2:eval("return box.info.election.state")}})

    t.assert_equals(c3:eval("return i3"),
        {"box.election",
            {term = cg.instance_3:eval("return box.info.election.term"),
            leader = cg.instance_3:eval("return box.info.election.leader"),
            is_ro = cg.instance_3:eval("return box.info.ro"),
            role = cg.instance_3:eval("return box.info.election.state")}})

    c1:close()
    c2:close()
    c3:close()
end
