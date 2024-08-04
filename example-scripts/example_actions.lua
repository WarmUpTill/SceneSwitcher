-------------------------------------------------------------------------------
-- Since lua does not support threads natively you will have to be careful to
-- to not block the main OBS script thread with long running actions.
--
-- Also note that due to this limitation only one of the actions defined in this
-- script can be executed at a time.
-- If multiple actions would be executed at the same time they will be executed
-- sequentially instead.
--
-- Consider switching to python instead, if those limitations will be a problem
-------------------------------------------------------------------------------

obs = obslua

-------------------------------------------------------------------------------

-- Simple example action callback

function my_lua_action(data)
    obs.script_log(obs.LOG_WARNING, "hello from lua!")
end

-------------------------------------------------------------------------------

-- Action callback demonstrating the interacting with variables

counter = 0

function variable_lua_action(data)
    local value = advss_get_variable_value("variable")
    if value ~= nil then
        obs.script_log(obs.LOG_WARNING, string.format("variable has value: %s", value))
    end

    counter = counter + 1
    advss_set_variable_value("variable", counter)
end

-------------------------------------------------------------------------------

-- Example condition randomly changing its value every second
set_condition_function = nil
math.randomseed(os.time())

function timer_callback()
    if set_condition_function ~= nil then
        local value = math.random(0, 1) == 0
        set_condition_function(value)
    end
end

obs.timer_add(timer_callback, 1000)

-------------------------------------------------------------------------------

function script_load(settings)
    -- Register an example action
    advss_register_action("My simple Lua action", my_lua_action)

    -- This example action demonstrates how to interact with variables
    advss_register_action("My variable Lua action", variable_lua_action)

    -- Register an example condition
    set_condition_function = advss_register_condition("My Lua condition")
end

function script_unload()
    -- Deregistering is useful if you plan on reloading the script files
    advss_deregister_action("My simple Lua action")
    advss_deregister_action("My variable Lua action")

    advss_deregister_condition("My Lua condition")
end

-------------------------------------------------------------------------------

-- Advanced Scene Switcher helper functions below
-- Usually you should not have to modify this code
-- Simply copy paste it into your scripts

-------------------------------------------------------------------------------

-- Actions

-- The advss_register_action() function is used to register custom actions
-- It takes three arguments:
-- 1. The name of the new action type
-- 2. The function callback to be ran when the action is executed
-- 3. The optional boolean flag for the "blocking" parameter
-- If the "blocking" parameter of advss_register_action() is set to false the
-- Advanced Scene Switcher plugin will not wait for the provided script function
-- to complete before continuing with the next action in a given macro
function advss_register_action(name, callback, blocking)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()

    if not blocking then
        blocking = true
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

    -- Lua does not support threads natively so we will call the provided
    -- callback function directly
    local run_helper = (
        function(data)
            local completion_signal_name = obs.calldata_string(data, "completion_signal_name")
            local id = obs.calldata_int(data, "completion_id")

            callback(data)

            local reply_data = obs.calldata_create()
            obs.calldata_set_int(reply_data, "completion_id", id)
            local signal_handler = obs.obs_get_signal_handler()
            obs.signal_handler_signal(signal_handler, completion_signal_name, reply_data)
            obs.calldata_destroy(reply_data)
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

-- Conditions

-- The advss_register_condition() function is used to register custom conditions
-- It takes one argument:
-- The name of the new condition type
-- It returns the function to call to change the value of the condition
function advss_register_condition(name)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_register_script_condition", data)

    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, "failed to register custom condition \"" + name + "\"")
        obs.calldata_destroy(data)
        return
    end

    local signal_name = obs.calldata_string(data, "change_value_signal_name")
    obs.calldata_destroy(data)

    function set_condition_value_func(value)
        local condition_data = obs.calldata_create()
        obs.calldata_set_bool(condition_data, "condition_value", value)
        local signal_handler = obs.obs_get_signal_handler()
        obs.signal_handler_signal(signal_handler, signal_name, condition_data)
        obs.calldata_destroy(condition_data)
    end
    return set_condition_value_func
end

function advss_deregister_condition(name)
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_deregister_script_condition", data)

    success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, "failed to deregister custom condition \"" + name + "\"")
    end
    obs.calldata_destroy(data)
end

-- Variables

-- The advss_get_variable_value() function can be used to query the value of a
-- variable with a given name.
-- None is returned in case the variable does not exist.
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

-- The advss_set_variable_value() function can be used to set the value of a
-- variable with a given name.
-- True is returned if the operation was successful.
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
    return success
end