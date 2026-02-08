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
        tick_log_interval = 1000000,
        time_wheel_default_slot_count = 100,
    },
    room = {
        empty_destroy_delay_ms = 300000,
    },
    bot = {
        id_base = 20000000,
        default_name = "Bot",
        starting_chips = 10000000,
        think_time_ms = 800,
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

local function assert_string(value, name)
    if type(value) ~= "string" then
        error(name .. " must be string")
    end
end

assert_int(cfg.console.code_page, "console.code_page", 0, 65535)
assert_int(cfg.db.port, "db.port", 1, 65535)
assert_int(cfg.net.listen_port, "net.listen_port", 1, 65535)
assert_int(cfg.net.backlog, "net.backlog", 1, 65535)
assert_int(cfg.loop.fixed_time_step_ms, "loop.fixed_time_step_ms", 1, 60000)
assert_int(cfg.loop.net_poll_interval_ms, "loop.net_poll_interval_ms", 1, 60000)
assert_int(cfg.loop.tick_log_interval, "loop.tick_log_interval", 1, 1000000)
assert_int(cfg.loop.time_wheel_default_slot_count, "loop.time_wheel_default_slot_count", 1, 1000)
assert_int(cfg.room.empty_destroy_delay_ms, "room.empty_destroy_delay_ms", 0, 3600000)
assert_int(cfg.bot.id_base, "bot.id_base", 1, 2000000000)
assert_string(cfg.bot.default_name, "bot.default_name")
assert_int(cfg.bot.starting_chips, "bot.starting_chips", 0, 2000000000)
assert_int(cfg.bot.think_time_ms, "bot.think_time_ms", 0, 60000)

return cfg
