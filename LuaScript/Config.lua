local cfg = {
    console = { code_page = 936 },
    db = {
        host = "127.0.0.1",
        port = 33060,
        user = "root",
        password = "1QAZ2wsx",
        schema = "wkr_server_schema",
    },
    net = {
        listen_port = 4242,
        bind_addr = "0.0.0.0",
        backlog = 128,
    },
    loop = {
        fixed_time_step_ms = 200,
        net_poll_interval_ms = 50,
    },
}

local function assert_int(value, name, min, max)
    if type(value) ~= "number" or value % 1 ~= 0 then
        error(name .. " must be int")
    end
    if min and value < min then
        error(name .. " < " .. min)
    end
    if max and value > max then
        error(name .. " > " .. max)
    end
end

assert_int(cfg.console.code_page, "console.code_page", 0, 65535)
assert_int(cfg.db.port, "db.port", 1, 65535)
assert_int(cfg.net.listen_port, "net.listen_port", 1, 65535)
assert_int(cfg.net.backlog, "net.backlog", 1, 65535)
assert_int(cfg.loop.fixed_time_step_ms, "loop.fixed_time_step_ms", 1, 60000)
assert_int(cfg.loop.net_poll_interval_ms, "loop.net_poll_interval_ms", 1, 60000)

return cfg
