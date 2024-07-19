import obspython as obs
import threading
import time

counter = 0
set_condition_value = None

# Example actions
def my_python_action(data):
    obs.script_log(obs.LOG_WARNING, "hello from python!")

def blocking_python_action_example(data):
    obs.script_log(obs.LOG_WARNING, "my blocking python function is starting!")
    time.sleep(3)
    obs.script_log(obs.LOG_WARNING, "my blocking python function is finished!")

def variable_python_action(data):
    value = advss_get_variable_value("variable")
    if value is not None:
        obs.script_log(obs.LOG_WARNING, "variable has value: " + value)

    global counter
    counter += 1
    advss_set_variable_value("variable", str(counter))

# Register the example actions
def script_load(settings):
    advss_register_action("My custom Python Action", my_python_action)
    advss_register_action("My blocking Python Action", blocking_python_action_example, True)
    advss_register_action("My variable Python Action", variable_python_action)

    global set_condition_value
    set_condition_value = advss_register_condition("My Python condition")
    set_condition_value(True)

def script_unload():
    advss_deregister_action("My custom Python Action")
    advss_deregister_action("My blocking Python Action")
    advss_deregister_action("My variable Python Action")
    
    advss_deregister_condition("My Python condition")

# Advanced Scene Switcher helpers below
# Usually you should not have to modify this code and can copy it to your scripts as is

### Actions ###

def advss_register_action(name, callback, blocking = False):
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
            signal_handler = obs.obs_get_signal_handler()
            obs.calldata_set_int(data, "completion_id", id)
            obs.signal_handler_signal(signal_handler, completion_signal_name, data)
            
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

### Conditions ###

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
    def set_condition_value_func(value):
        data = obs.calldata_create()
        obs.calldata_set_bool(data, "value", value)
        signal_handler = obs.obs_get_signal_handler()
        obs.signal_handler_signal(signal_handler, signal_name, None)
        obs.calldata_destroy(data)

    obs.calldata_destroy(data)
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

### Variables ###

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
