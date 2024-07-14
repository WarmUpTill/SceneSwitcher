obs           = obslua

function my_lua_action(data)
    obs.script_log(obs.LOG_WARNING, "hello from lua!")
end

function busy_sleep(seconds)
    local start = os.clock()
    while os.clock() - start <= seconds do end
end

function blocking_lua_action_example(data)
    obs.script_log(obs.LOG_WARNING, "my blocking lua function is starting!")
    busy_sleep(3)
    obs.script_log(obs.LOG_WARNING, "my blocking lua function is finished!")
    local signal_handler = obs.obs_get_signal_handler()
    local signal_name = obs.calldata_string(data, "completion_signal_name")
    obs.signal_handler_signal(signal_handler, signal_name, nil)
end

function register_action(name, callback, blocking)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()
    
    if not blocking then
        blocking = false
    end

    obs.calldata_set_string(data, "name", name)
    obs.calldata_set_bool(data, "blocking", blocking)
    obs.proc_handler_call(proc_handler, "advss_register_script_action", data)
    
    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, string.format("failed to register custom action \"%s\"", name))
        return
    end
    
    local signal_name = obs.calldata_string(data, "trigger_signal_name")
    local signal_handler = obs.obs_get_signal_handler()
    obs.signal_handler_connect(signal_handler, signal_name, callback)

    obs.calldata_destroy(data)
end

function script_load(settings)
    register_action("My custom Lua Action", my_lua_action)
    register_action("My blocking Lua Action", blocking_lua_action_example, true)
end
