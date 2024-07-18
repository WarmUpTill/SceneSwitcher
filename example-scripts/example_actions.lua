obs = obslua

counter = 0

-- Example actions
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
end

function variable_lua_action(data)
    local value = advss_get_variable_value("variable")
    if value ~= nil then
        obs.script_log(obs.LOG_WARNING, string.format("variable has value: %s", value))
    end

    counter = counter + 1
    advss_set_variable_value("variable", counter)
end

-- Register the example actions
function script_load(settings)
    advss_register_action("My custom Lua Action", my_lua_action)
    advss_register_action("My blocking Lua Action", blocking_lua_action_example, true)
    advss_register_action("My variable Lua Action", variable_lua_action)
end

function script_unload()
    advss_deregister_action("My custom Lua Action")
    advss_deregister_action("My blocking Lua Action")
    advss_deregister_action("My variable Lua Action")
end

-- Advanced Scene Switcher helpers below
-- Usually you should not have to modify this code and can copy it to your scripts as is
function advss_register_action(name, callback, blocking)
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
        obs.calldata_destroy(data)
        return
    end

    -- Run action in coroutine to avoid blocking main OBS signal handler
    -- Action completion will be indicated via signal completion_signal_name
    -- TODO: this does not work / seems like lua does not support threads natively
    local run_helper = (
        function(data)
            local completion_signal_name = obs.calldata_string(data, "completion_signal_name")
            co = coroutine.create(
                function(data)
                    callback(data)
                    local signal_handler = obs.obs_get_signal_handler()
                    obs.signal_handler_signal(signal_handler, completion_signal_name, nil)
                end
            )
            coroutine.resume(co)
        end
    )

    local signal_name = obs.calldata_string(data, "trigger_signal_name")
    local signal_handler = obs.obs_get_signal_handler()
    obs.signal_handler_connect(signal_handler, signal_name, run_helper)

    obs.calldata_destroy(data)
end

function advss_deregister_action(name)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()
    
    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_deregister_script_action", data)
    
    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, "failed to deregister custom action \"" + name + "\"")
    end

    obs.calldata_destroy(data)
end

function advss_get_variable_value(name)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()
    
    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_get_variable_value", data)
    
    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, "failed to get value for variable \"" + name + "\"")
        obs.calldata_destroy(data)
        return None
    end

    local value = obs.calldata_string(data, "value")
    
    obs.calldata_destroy(data)
    return value
end

function advss_set_variable_value(name, value)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()
    
    obs.calldata_set_string(data, "name", name)
    obs.calldata_set_string(data, "value", value)
    obs.proc_handler_call(proc_handler, "advss_set_variable_value", data)
    
    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, "failed to set value for variable \"" + name + "\"")
    end

    obs.calldata_destroy(data)
end