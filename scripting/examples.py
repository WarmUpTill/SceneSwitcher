import obspython as obs
import threading  # Required by advss helpers
import random  # Required for example condition

###############################################################################

# Simple action callback example


def my_python_action(data):
    obs.script_log(obs.LOG_WARNING, "hello from python!")


###############################################################################

# Action showcasing how to provide configurable settings


def get_action_properties():
    props = obs.obs_properties_create()
    obs.obs_properties_add_text(props, "name", "Name", obs.OBS_TEXT_DEFAULT)
    return props


def get_action_defaults():
    default_settings = obs.obs_data_create()
    obs.obs_data_set_default_string(default_settings, "name", "John")
    return default_settings


def my_python_settings_action(data):
    name = obs.obs_data_get_string(data, "name")
    obs.script_log(obs.LOG_WARNING, f"hello {name} from python!")


###############################################################################

# Action callback demonstrating the interacting with variables

counter = 0


def variable_python_action(data):
    value = advss_get_variable_value("variable")
    if value is not None:
        obs.script_log(obs.LOG_WARNING, f"variable has value: {value}")

    global counter
    counter += 1
    advss_set_variable_value("variable", str(counter))


###############################################################################

# Example condition randomly returning true or false based on user configured
# probability value


def get_condition_properties():
    props = obs.obs_properties_create()
    obs.obs_properties_add_float(
        props, "probability", "Probability of returning true", 0, 100, 0.1
    )
    return props


def get_condition_defaults():
    default_settings = obs.obs_data_create()
    obs.obs_data_set_default_double(default_settings, "probability", 33.3)
    return default_settings


def my_python_condition(data):
    target = obs.obs_data_get_double(data, "probability")
    value = random.uniform(0, 100)
    return value <= target


###############################################################################


def script_load(settings):
    # Register an example action
    advss_register_action("My simple Python action", my_python_action)

    # Register an example action with settings
    advss_register_action(
        "My Python action with settings",
        my_python_settings_action,
        get_action_properties,
        get_action_defaults(),
    )

    # This example action demonstrates how to interact with variables
    advss_register_action("My variable Python action", variable_python_action)

    # Register an example condition
    advss_register_condition(
        "My Python condition",
        my_python_condition,
        get_condition_properties,
        get_condition_defaults(),
    )


def script_unload():
    # Deregistering is useful if you plan on reloading the script files
    advss_deregister_action("My simple Python action")
    advss_deregister_action("My Python action with settings")
    advss_deregister_action("My variable Python action")

    advss_deregister_condition("My Python condition")


###############################################################################

# Advanced Scene Switcher helper functions below:
# Usually you should not have to modify this code.
# Simply copy paste it into your scripts.

###############################################################################
# Actions
###############################################################################


# The advss_register_action() function is used to register custom actions
# It takes the following arguments:
# 1. The name of the new action type.
# 2. The function callback which should run when the action is executed.
# 3. The optional function callback which return the properties to display the
#    settings of this action type.
# 4. The optional default_settings pointer used to set the default settings of
#    newly created actions.
#    The pointer must not be freed within this script.
def advss_register_action(name, callback, get_properties=None, default_settings=None):
    advss_register_segment_type(True, name, callback, get_properties, default_settings)


def advss_deregister_action(name):
    advss_deregister_segment(True, name)


###############################################################################
# Conditions
###############################################################################


# The advss_register_condition() function is used to register custom conditions
# It takes the following arguments:
# 1. The name of the new condition type.
# 2. The function callback which should run when the condition is executed.
# 3. The optional function callback which return the properties to display the
#    settings of this condition type.
# 4. The optional default_settings pointer used to set the default settings of
#    newly created condition.
#    The pointer must not be freed within this script.
def advss_register_condition(
    name, callback, get_properties=None, default_settings=None
):
    advss_register_segment_type(False, name, callback, get_properties, default_settings)


def advss_deregister_condition(name):
    advss_deregister_segment(False, name)


###############################################################################
# (De)register helpers
###############################################################################


def advss_register_segment_type(
    is_action, name, callback, get_properties, default_settings
):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)
    obs.calldata_set_ptr(data, "default_settings", default_settings)

    register_proc = (
        "advss_register_script_action"
        if is_action
        else "advss_register_script_condition"
    )
    obs.proc_handler_call(proc_handler, register_proc, data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        segment_type = "action" if is_action else "condition"
        log_msg = f'failed to register custom {segment_type} "{name}"'
        obs.script_log(obs.LOG_WARNING, log_msg)
        obs.calldata_destroy(data)
        return

    # Run in separate thread to avoid blocking main OBS signal handler.
    # Operation completion will be indicated via signal completion_signal_name.
    def run_helper(data):
        completion_signal_name = obs.calldata_string(data, "completion_signal_name")
        id = obs.calldata_int(data, "completion_id")

        def thread_func(settings):
            settings = obs.obs_data_create_from_json(
                obs.calldata_string(data, "settings")
            )
            callback_result = callback(settings)
            if is_action:
                callback_result = True

            reply_data = obs.calldata_create()
            obs.calldata_set_int(reply_data, "completion_id", id)
            obs.calldata_set_bool(reply_data, "result", callback_result)
            signal_handler = obs.obs_get_signal_handler()
            obs.signal_handler_signal(
                signal_handler, completion_signal_name, reply_data
            )
            obs.obs_data_release(settings)
            obs.calldata_destroy(reply_data)

        threading.Thread(target=thread_func, args={data}).start()

    def properties_helper(data):
        if get_properties is not None:
            properties = get_properties()
        else:
            properties = None
        obs.calldata_set_ptr(data, "properties", properties)

    trigger_signal_name = obs.calldata_string(data, "trigger_signal_name")
    property_signal_name = obs.calldata_string(data, "properties_signal_name")

    signal_handler = obs.obs_get_signal_handler()
    obs.signal_handler_connect(signal_handler, trigger_signal_name, run_helper)
    obs.signal_handler_connect(signal_handler, property_signal_name, properties_helper)

    obs.calldata_destroy(data)


def advss_deregister_segment(is_action, name):
    proc_handler = obs.obs_get_proc_handler()
    data = obs.calldata_create()

    obs.calldata_set_string(data, "name", name)

    deregister_proc = (
        "advss_deregister_script_action"
        if is_action
        else "advss_deregister_script_condition"
    )

    obs.proc_handler_call(proc_handler, deregister_proc, data)

    success = obs.calldata_bool(data, "success")
    if success == False:
        segment_type = "action" if is_action else "condition"
        log_msg = f'failed to deregister custom {segment_type} "{name}"'
        obs.script_log(obs.LOG_WARNING, log_msg)

    obs.calldata_destroy(data)


###############################################################################
# Variables
###############################################################################


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
        obs.script_log(obs.LOG_WARNING, f'failed to get value for variable "{name}"')
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
        obs.script_log(obs.LOG_WARNING, f'failed to set value for variable "{name}"')

    obs.calldata_destroy(data)
    return success
