import obspython as obs
import threading
import random

# Simple example action callback
def my_python_action(data):
    obs.script_log(obs.LOG_WARNING, "hello from python!")

# Action callback demonstrating the interacting with variables
counter = 0

def variable_python_action(data):
    value = advss_get_variable_value("variable")
    if value is not None:
        obs.script_log(obs.LOG_WARNING, "variable has value: " + value)

    global counter
    counter += 1
    advss_set_variable_value("variable", str(counter))

# Example condition randomly changing its value every second
set_condition_function = None

def timer_callback():
    if set_condition_function is not None:
        value = random.randint(0, 1) == 0
        set_condition_function(value)

obs.timer_add(timer_callback, 1000)

###############################################################################

def script_load(settings):
    # Register an example action
    advss_register_action("My simple Python Action", my_python_action)

    # This example action demonstrates how to interact with variables
    advss_register_action("My variable Python Action", variable_python_action)

    # Register an example condition
    global set_condition_function
    set_condition_function = advss_register_condition("My Python condition")

def script_unload():
    # Deregistering is useful if you plan on reloading the script files
    advss_deregister_action("My simple Python Action")
    advss_deregister_action("My variable Python Action")

    advss_deregister_condition("My Python condition")

###############################################################################

# Advanced Scene Switcher helper functions below
# Usually you should not have to modify this code
# Simply copy paste it into your scripts

###############################################################################

# Actions

# The advss_register_action() function is used to register custom actions
# It takes three arguments:
# 1. The name of the new action type
# 2. The function callback to be ran when the action is executed
# 3. The optional boolean flag for the "blocking" parameter
# If the "blocking" parameter of advss_register_action() is set to false the
# Advanced Scene Switcher plugin will not wait for the provided script function
# to complete before continuing with the next action in a given macro
def advss_register_action(name, callback, blocking = True):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.calldata_set_bool(data, "blocking", blocking)
    obs.proc_handler_call(proc_handler, "advss_register_script_action", data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        obs.script_log(obs.LOG_WARNING, "failed to register custom action \"" + name + "\"")
        obs.calldata_destroy(data)
        return

    # Run action in coroutine to avoid blocking main OBS signal handler
    # Action completion will be indicated via signal completion_signal_name
    def run_helper(data):
        completion_signal_name = obs.calldata_string(data, "completion_signal_name")
        id = obs.calldata_int(data, "completion_id")

        def thread_func(data):
            callback(data)

            reply_data = obs.calldata_create()
            obs.calldata_set_int(reply_data, "completion_id", id)
            signal_handler = obs.obs_get_signal_handler()
            obs.signal_handler_signal(signal_handler, completion_signal_name, reply_data)
            obs.calldata_destroy(reply_data)

        threading.Thread(target = thread_func, args = {data}).start()

    signal_name = obs.calldata_string(data, "trigger_signal_name")
    signal_handler = obs.obs_get_signal_handler()
    obs.signal_handler_connect(signal_handler, signal_name, run_helper)

    obs.calldata_destroy(data)

def advss_deregister_action(name):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_deregister_script_action", data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        obs.script_log(obs.LOG_WARNING, "failed to deregister custom action \"" + name + "\"")

    obs.calldata_destroy(data)

# Conditions

# The advss_register_condition() function is used to register custom conditions
# It takes one argument:
# The name of the new condition type
# It returns the function to call to change the value of the condition
def advss_register_condition(name):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_register_script_condition", data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        obs.script_log(obs.LOG_WARNING, "failed to register custom condition \"" + name + "\"")
        obs.calldata_destroy(data)
        return

    signal_name = obs.calldata_string(data, "change_value_signal_name")
    obs.calldata_destroy(data)

    def set_condition_value_func(value):
        condition_data = obs.calldata_create()
        obs.calldata_set_bool(condition_data, "condition_value", value)
        signal_handler = obs.obs_get_signal_handler()
        obs.signal_handler_signal(signal_handler, signal_name, condition_data)
        obs.calldata_destroy(condition_data)

    return set_condition_value_func

def advss_deregister_condition(name):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_deregister_script_condition", data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        obs.script_log(obs.LOG_WARNING, "failed to deregister custom condition \"" + name + "\"")

    obs.calldata_destroy(data)

# Variables

# The advss_get_variable_value() function can be used to query the value of a
# variable with a given name.
# None is returned in case the variable does not exist.
def advss_get_variable_value(name):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.proc_handler_call(proc_handler, "advss_get_variable_value", data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        obs.script_log(obs.LOG_WARNING, "failed to get value for variable \"" + name + "\"")
        obs.calldata_destroy(data)
        return None

    value = obs.calldata_string(data, "value")

    obs.calldata_destroy(data)
    return value

# The advss_set_variable_value() function can be used to set the value of a
# variable with a given name.
# True is returned if the operation was successful.
def advss_set_variable_value(name, value):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.calldata_set_string(data, "value", value)
    obs.proc_handler_call(proc_handler, "advss_set_variable_value", data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        obs.script_log(obs.LOG_WARNING, "failed to set value for variable \"" + name + "\"")

    obs.calldata_destroy(data)
    return success
