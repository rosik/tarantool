-- connect to replica from master
box.schema.user.grant('replicator', 'read,write,execute', 'universe')

-- get replica IP from master and connect to it
box.info.replication[2].upstream.peer
c = require('net.box').connect('replicator:password@172.17.0.3:3301')
