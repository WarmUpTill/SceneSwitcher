-------------------------------------------------------------------------------
-- Since lua does not support threads natively, you will have to be careful to
-- to not block the main OBS script thread with long running actions or 
-- conditions.
--
-- Also note that due to this limitation only one instance of the actions and
-- conditions defined in this script can be executed at a time.
-- If multiple actions or conditions would have been executed at the same time,
-- they will be executed sequentially instead.
--
-- Consider switching to python instead, if those limitations are a problem.
-------------------------------------------------------------------------------
obs = obslua

-------------------------------------------------------------------------------

-- Simple action callback example

function my_lua_action(data, instance_id)
    obs.script_log(obs.LOG_WARNING, "hello from lua!")
end

-------------------------------------------------------------------------------

-- Action showcasing how to provide configurable settings

-- Define the settings available for the user for this action type
function get_action_properties()
    local props = obs.obs_properties_create()
    obs.obs_properties_add_text(props, "name", "Name", obs.OBS_TEXT_DEFAULT)
    return props
end

-- Set default values for each setting
function get_action_defaults()
    local default_settings = obs.obs_data_create()
    obs.obs_data_set_default_string(default_settings, "name", "John")
    return default_settings
end

-- The settings for each instance of this action the will be passed in the
-- "data" argument
function my_lua_settings_action(data, instance_id)
    local name = obs.obs_data_get_string(data, "name")
    obs.script_log(obs.LOG_WARNING, string.format("hello %s from lua", name))
end

-------------------------------------------------------------------------------

-- Action callback demonstrating the interaction with variables

counter = 0

-- You can use "advss_get_variable_value()" to get the value of a variable
-- Use "advss_set_variable_value()" to set the value of a variable
function variable_lua_action(data, instance_id)
    local value = advss_get_variable_value("variable")
    if value ~= nil then
        obs.script_log(obs.LOG_WARNING, string.format("variable has value: %s", value))
    end

    counter = counter + 1
    advss_set_variable_value("variable", counter)
end

-------------------------------------------------------------------------------

-- Example condition randomly returning true or false based on user configured
-- probability value

math.randomseed(os.time())

-- Define the settings available to the user for this condition type
function get_condition_properties()
    local props = obs.obs_properties_create()
    obs.obs_properties_add_float(props, "probability", "Probability of returning true", 0, 100, 0.1)
    return props
end

-- Define default values for each setting
function get_condition_defaults()
    local default_settings = obs.obs_data_create()
    obs.obs_data_set_default_double(default_settings, "probability", 33.3)
    return default_settings
end

-- The settings for each instance of this condition the will be passed in the
-- "data" argument
function my_lua_condition(data)
    local target = obs.obs_data_get_double(data, "probability")
    local value = math.random(0, 100)
    return value <= target
end

-------------------------------------------------------------------------------

-- Example condition which demonstrates how to set macro properties.

-- Set the value of each macro property with the "advss_set_temp_var_value()"
-- function
--
-- The "instance_id" uniquely identifies each instance of this condition type
-- and needs to be passed to "advss_set_temp_var_value()"
function my_temp_var_lua_condition(data, instance_id)
    local value = math.random(0, 100)
    advss_set_temp_var_value("random_int", tonumber(value), instance_id)
    advss_set_temp_var_value("some_other_value", value, instance_id)
    return true
end

-------------------------------------------------------------------------------

function script_load(settings)
    -- Register an example action
    advss_register_action("My simple Lua action", my_lua_action, nil, nil, nil)

    -- Register an example action with settings
    advss_register_action("My Lua action with settings", my_lua_settings_action, get_action_properties,
        get_action_defaults(), nil)

    -- This example action demonstrates how to interact with variables
    advss_register_action("My variable Lua action", variable_lua_action, nil, nil, nil)

    -- Register an example condition
    advss_register_condition("My Lua condition", my_lua_condition, get_condition_properties, get_condition_defaults(),
        nil)

    -- Register an example condition which registers and sets macro properties
    local macro_property1 = {}
    macro_property1.id = "random_int"
    macro_property1.name = "Random number"
    macro_property1.description = "A random number generated in the range from 0 to 100"

    local macro_property2 = {}
    macro_property2.id = "some_other_value"
    macro_property2.name = "Another value"
    macro_property2.description = ""

    advss_register_condition("My Lua condition (Macro properties)", my_temp_var_lua_condition, nil, nil,
        {macro_property1, macro_property2})
end

function script_unload()
    -- Deregistering is useful if you plan on reloading the script files
    advss_deregister_action("My simple Lua action")
    advss_deregister_action("My Lua action with settings")
    advss_deregister_action("My variable Lua action")

    advss_deregister_condition("My Lua condition")
    advss_deregister_condition("My Lua condition (Macro properties)")
end

-------------------------------------------------------------------------------

-- Advanced Scene Switcher helper functions below:
-- Usually you should not have to modify this code.
-- Simply copy paste it into your scripts.

-------------------------------------------------------------------------------
-- Actions
-------------------------------------------------------------------------------

-- The advss_register_action() function is used to register custom actions
-- It takes the following arguments:
-- 1. The name of the new action type.
-- 2. The function callback which should run when the action is executed.
-- 3. The optional function callback which return the properties to display the
--    settings of this action type.
-- 4. The optional default_settings pointer used to set the default settings of
--    newly created actions.
--    The pointer must not be freed within this script.
-- 5. The optional list of macro properties associated with this action type.
--    You can set values using advss_set_temp_var_value().
function advss_register_action(name, callback, get_properties, default_settings, macro_properties)
    advss_register_segment_type(true, name, callback, get_properties, default_settings, macro_properties)
end

function advss_deregister_action(name)
    advss_deregister_segment(true, name)
end

-------------------------------------------------------------------------------
-- Conditions
-------------------------------------------------------------------------------

-- The advss_register_condition() function is used to register custom conditions
-- It takes the following arguments:
-- 1. The name of the new condition type.
-- 2. The function callback which should run when the condition is executed.
-- 3. The optional function callback which return the properties to display the
--    settings of this condition type.
-- 4. The optional default_settings pointer used to set the default settings of
--    newly created condition.
--    The pointer must not be freed within this script.
-- 5. The optional list of macro properties associated with this condition type.
--    You can set values using advss_set_temp_var_value().
function advss_register_condition(name, callback, get_properties, default_settings, macro_properties)
    advss_register_segment_type(false, name, callback, get_properties, default_settings, macro_properties)
end

function advss_deregister_condition(name)
    advss_deregister_segment(false, name)
end

-------------------------------------------------------------------------------
-- (De)register helpers
-------------------------------------------------------------------------------

function advss_register_segment_type(is_action, name, callback, get_properties, default_settings, macro_properties)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.calldata_set_ptr(data, "default_settings", default_settings)

    local register_proc = is_action and "advss_register_script_action" or "advss_register_script_condition"
    obs.proc_handler_call(proc_handler, register_proc, data)

    local success = obs.calldata_bool(data, "success")
    if success == false then
        local segment_type = is_action and "action" or "condition"
        obs.script_log(obs.LOG_WARNING, string.format("failed to register custom %s \"%s\"", segment_type, name))
        obs.calldata_destroy(data)
        return
    end

    -- Lua does not support threads natively.
    -- So, we will call the provided callback function directly.
    local run_helper = (function(data)
        local completion_signal_name = obs.calldata_string(data, "completion_signal_name")
        local completion_id = obs.calldata_int(data, "completion_id")
        local instance_id = obs.calldata_int(data, "instance_id")
        local settings = obs.obs_data_create_from_json(obs.calldata_string(data, "settings"))

        local callback_result = callback(settings, instance_id)
        if is_action then
            callback_result = true
        end

        obs.obs_data_release(settings)

        local reply_data = obs.calldata_create()
        obs.calldata_set_int(reply_data, "completion_id", completion_id)
        obs.calldata_set_bool(reply_data, "result", callback_result)
        local signal_handler = obs.obs_get_signal_handler()
        obs.signal_handler_signal(signal_handler, completion_signal_name, reply_data)
        obs.calldata_destroy(reply_data)
    end)

    local properties_helper = (function(data)
        if get_properties ~= nil then
            obs.calldata_set_ptr(data, "properties", get_properties())
        else
            obs.calldata_set_ptr(data, "properties", nil)
        end
    end)

    -- Helper to register the macro properties every time a new instance of the
    -- macro segment is created.
    local register_temp_vars_helper = (function(data)
        local id = obs.calldata_int(data, "instance_id")
        local proc_handler = obs.obs_get_proc_handler()
        local data = obs.calldata_create()
        for _, prop in ipairs(macro_properties) do
            obs.calldata_set_string(data, "temp_var_id", prop.id)
            obs.calldata_set_string(data, "temp_var_name", prop.name)
            obs.calldata_set_string(data, "temp_var_help", prop.description)
            obs.calldata_set_int(data, "instance_id", id)

            obs.proc_handler_call(proc_handler, "advss_register_temp_var", data)

            local success = obs.calldata_bool(data, "success")
            if success == false then
                local segment_type = is_action and "action" or "condition"
                log_msg = f 'failed to register macro property {prop.id} for {segment_type} "{name}"'
                obs.script_log(obs.LOG_WARNING, log_msg)
                obs.script_log(obs.LOG_WARNING, string.format("failed to register macro property %s for %s \"%s\"",
                    prop.id, segment_type, name))
            end
        end
        obs.calldata_destroy(data)
    end)

    local signal_name = obs.calldata_string(data, "trigger_signal_name")
    local property_signal_name = obs.calldata_string(data, "properties_signal_name")
    local new_instance_signal_name = obs.calldata_string(data, "new_instance_signal_name")
    local signal_handler = obs.obs_get_signal_handler()
    obs.signal_handler_connect(signal_handler, signal_name, run_helper)
    obs.signal_handler_connect(signal_handler, property_signal_name, properties_helper)
    if macro_properties ~= nil then
        obs.signal_handler_connect(signal_handler, new_instance_signal_name, register_temp_vars_helper)
    end
    obs.calldata_destroy(data)
end

function advss_deregister_segment(is_action, name)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)

    local deregister_proc = is_action and "advss_deregister_script_action" or "advss_deregister_script_condition"
    obs.proc_handler_call(proc_handler, deregister_proc, data)

    local success = obs.calldata_bool(data, "success")
    if success == false then
        local segment_type = is_action and "action" or "condition"
        obs.script_log(obs.LOG_WARNING, string.format("failed to deregister custom %s \"%s\"", segment_type, name))
    end

    obs.calldata_destroy(data)
end

-------------------------------------------------------------------------------
-- Macro properties (temporary variables)
-------------------------------------------------------------------------------

function advss_set_temp_var_value(temp_var_id, value, instance_id)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()

    obs.calldata_set_string(data, "temp_var_id", tostring(temp_var_id))
    obs.calldata_set_string(data, "value", tostring(value))
    obs.calldata_set_int(data, "instance_id", tonumber(instance_id))
    obs.proc_handler_call(proc_handler, "advss_set_temp_var_value", data)

    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, string.format("failed to set value for macro property \"%s\"", temp_var_id))
    end

    obs.calldata_destroy(data)
end

-------------------------------------------------------------------------------
-- Variables
-------------------------------------------------------------------------------

-- The advss_get_variable_value() function can be used to query the value of a
-- variable with a given name.
-- nil is returned in case the variable does not exist.
function advss_get_variable_value(name)
    local proc_handler = obs.obs_get_proc_handler()
    local data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_get_variable_value", data)

    local success = obs.calldata_bool(data, "success")
    if success == false then
        obs.script_log(obs.LOG_WARNING, string.format("failed to get value for variable \"%s\"", name))
        obs.calldata_destroy(data)
        return nil
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
        obs.script_log(obs.LOG_WARNING, string.format("failed to set value for variable \"%s\"", name))
    end

    obs.calldata_destroy(data)
    return success
end
