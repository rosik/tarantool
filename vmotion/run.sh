# create standalone directories with files

mkdir master
cat >master/master.lua <<EOF
-- файл экземпляра для мастера
box.cfg{
  listen = 3301,
  replication = {'replicator:password@MASTER_IP:3301',  -- URI мастера
                 'replicator:password@REPLICA_IP:3301'}, -- URI реплики
  read_only = false
}
box.once("schema", function()
  box.schema.user.create('replicator', {password = 'password'})
  box.schema.user.grant('replicator', 'replication') -- настроить роль для репликации
  box.schema.space.create("test")
  box.space.test:create_index("primary")
  print('box.once executed on master')
end)
EOF

mkdir replica
cat >replica/replica.lua <<EOF
-- файл экземпляра для реплики
box.cfg{
  listen = 3301,
  replication = {'replicator:password@MASTER_IP:3301',  -- URI мастера
                 'replicator:password@REPLICA_IP:3301'}, -- URI реплики
  read_only = true
}
box.once("schema", function()
  box.schema.user.create('replicator', {password = 'password'})
  box.schema.user.grant('replicator', 'replication') -- настроить роль для репликации
  box.schema.space.create("test")
  box.space.test:create_index("primary")
  print('box.once executed on replica')
end)
EOF

# start master
cd master
docker run -v $PWD:/source -w /source --name tnt_master -ti tarantool/tarantool:2 /bin/sh

# start replica
cd replica
docker run -v $PWD:/source -w /source --name tnt_replica_1 -ti tarantool/tarantool:2 /bin/sh

# check IPs of the containers and
mip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' tnt_master)
rip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' tnt_replica_1)

for f in master/master.lua replica/replica.lua ; do
    sed "s#MASTER_IP#$mip#g" -i $f
    sed "s#REPLICA_IP#$rip#g" -i $f
done

# start in containers master and replica tarantools as consoles
cat master.lua
tarantool
> <copy/paste replica.lua>

cat master.lua
tarantool
> <copy/paste replica.lua>

# try in tnt consoles
box.info

# try steps from
#   https://www.tarantool.io/ru/doc/latest/book/replication/repl_bootstrap/#controlled-failover

# setup needed code routines
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

# connect to replica from master
box.schema.user.grant('replicator', 'read,write,execute', 'universe')

# get replica IP from master and connect to it
box.info.replication[2].upstream.peer
c = require('net.box').connect('replicator:password@172.17.0.3:3301')

# setup variables
results_replica = '0'
results_master = '0'
bsize = 1000
loops = 100

# cleanup tuples
--for id = 1,bsize*loops do spec_delete(box.space.test, id) end

# insert/delete tuples
start_pos = #box.space.test:select()
for num = 1,loops do
    bpos = start_pos + bsize * (num - 1)
    for id = 1,bsize do spec_insert(box.space.test, bpos + id) end
    for id = 1,bsize,num do spec_delete(box.space.test, bpos + id) end
    -- collect statistics on replica
    results_replica = results_replica .. ', ' .. c:eval([[ return #box.space.test:select() ]])
    -- collect statistics on master
    results_master = results_master .. ', ' .. #box.space.test:select()
end

# on replica get current tuples number on "replica - master - replica"
print(#box.space.test:select() .. " " .. c:eval([[ return #box.space.test:select() ]]) .. " " .. #box.space.test:select())
