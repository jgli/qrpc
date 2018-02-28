#================================================
#        qrpc library
#================================================

cc_library(
    name = 'qrpc',

    srcs = [
        'util/coding.cc',
        'util/crc32c.cc',
        'util/event_queue.cc',
        'util/fs.cc',
        'util/logging.cc',
        'util/md5.cc',
        'util/random.cc',
        'util/socket.cc',
        'util/thread.cc',
        'util/thread_pool.cc',
        'util/timer.cc',
        'util/zk_manager.cc',
        'rpc/builtin.cc',
        'rpc/channel.cc',
        'rpc/channel_impl.cc',
        'rpc/closure.cc',
        'rpc/command.cc',
        'rpc/compressor.cc',
        'rpc/connection.cc',
        'rpc/controller.cc',
        'rpc/controller_client.cc',
        'rpc/controller_server.cc',
        'rpc/errno.cc',
        'rpc/listener.cc',
        'rpc/message.cc',
        'rpc/server.cc',
        'rpc/server_impl.cc',
        'rpc/worker.cc',
    ],

    deps = [
        ':qrpc_internal_proto',
        '//src/util:util',
        '//thirdparty/lz4/lib:lz4',
        '//thirdparty/snappy:snappy',
        '//thirdparty/libevent/lib:event2',
        '//thirdparty/zookeeper_client/lib:zookeeper_st',
        '#pthread',
    ],

    extra_cppflags = [
    ],
)

proto_library(
    name = 'qrpc_internal_proto',

    srcs = [
        'rpc/builtin.proto',
        'rpc/message.proto',
    ],

    deps = [
    ],
)

#================================================
#        qrpc example
#================================================

proto_library(
    name = 'qrpc_example_proto',

    srcs = [
        'example/echo.proto',
    ],

    deps = [
    ],
)

cc_binary(
    name = 'qrpc_exam_cli',
    srcs = [
        'example/cli.cc',
    ],
    deps = [
        ':qrpc',
        ':qrpc_example_proto',
    ],
)

cc_binary(
    name = 'qrpc_exam_sync_srv',
    srcs = [
        'example/srv_sync.cc',
    ],
    deps = [
        ':qrpc',
        ':qrpc_example_proto',
    ],
)

cc_binary(
    name = 'qrpc_exam_async_srv',
    srcs = [
        'example/srv_async.cc',
    ],
    deps = [
        ':qrpc',
        ':qrpc_example_proto',
    ],
)

#================================================
#        qrpc benchmark
#================================================
