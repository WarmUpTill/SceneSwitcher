#include "source-interaction-step.hpp"
#include "obs-module-helper.hpp"
#include "variable.hpp"

#include <obs-interaction.h>

#include <chrono>
#include <thread>

#include <QString>

namespace advss {

static QString varOrNum(const NumberVariable<int> &v)
{
	if (!v.IsFixedType()) {
		return QString::fromStdString(
			GetWeakVariableName(v.GetVariable()));
	}
	return QString::number(v.GetFixedValue());
}

bool SourceInteractionStep::Save(obs_data_t *obj) const
{
	obs_data_set_int(obj, "type", static_cast<int>(type));
	x.Save(obj, "x");
	y.Save(obj, "y");
	obs_data_set_int(obj, "button", static_cast<int>(button));
	obs_data_set_bool(obj, "mouseUp", mouseUp);
	clickCount.Save(obj, "clickCount");
	wheelDeltaX.Save(obj, "wheelDeltaX");
	wheelDeltaY.Save(obj, "wheelDeltaY");
	nativeVkey.Save(obj, "nativeVkey");
	obs_data_set_int(obj, "modifiers", modifiers);
	obs_data_set_bool(obj, "keyUp", keyUp);
	text.Save(obj, "text");
	waitMs.Save(obj, "waitMs");
	return true;
}

bool SourceInteractionStep::Load(obs_data_t *obj)
{
	type = static_cast<Type>(obs_data_get_int(obj, "type"));
	x.Load(obj, "x");
	y.Load(obj, "y");
	button = static_cast<obs_mouse_button_type>(
		obs_data_get_int(obj, "button"));
	mouseUp = obs_data_get_bool(obj, "mouseUp");
	clickCount.Load(obj, "clickCount");
	wheelDeltaX.Load(obj, "wheelDeltaX");
	wheelDeltaY.Load(obj, "wheelDeltaY");
	nativeVkey.Load(obj, "nativeVkey");
	modifiers = static_cast<uint32_t>(obs_data_get_int(obj, "modifiers"));
	keyUp = obs_data_get_bool(obj, "keyUp");
	text.Load(obj, "text");
	waitMs.Load(obj, "waitMs");
	return true;
}

std::string SourceInteractionStep::ToString() const
{
	switch (type) {
	case Type::MOUSE_MOVE:
		return QString(obs_module_text(
				       "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.mouseMove"))
			.arg(varOrNum(x))
			.arg(varOrNum(y))
			.toStdString();
	case Type::MOUSE_CLICK: {
		const char *btnKey =
			(button == MOUSE_LEFT)
				? "AdvSceneSwitcher.action.sourceInteraction.button.left"
			: (button == MOUSE_MIDDLE)
				? "AdvSceneSwitcher.action.sourceInteraction.button.middle"
				: "AdvSceneSwitcher.action.sourceInteraction.button.right";
		const char *dirKey =
			mouseUp ? "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.mouseUp"
				: "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.mouseDown";
		QString result =
			QString(obs_module_text(
					"AdvSceneSwitcher.action.sourceInteraction.step.listEntry.mouseClick"))
				.arg(obs_module_text(dirKey))
				.arg(obs_module_text(btnKey))
				.arg(varOrNum(x))
				.arg(varOrNum(y));
		if (!clickCount.IsFixedType() ||
		    clickCount.GetFixedValue() > 1) {
			result +=
				QString(obs_module_text(
						"AdvSceneSwitcher.action.sourceInteraction.step.listEntry.mouseClickCount"))
					.arg(varOrNum(clickCount));
		}
		return result.toStdString();
	}
	case Type::MOUSE_WHEEL:
		return QString(obs_module_text(
				       "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.mouseWheel"))
			.arg(varOrNum(x))
			.arg(varOrNum(y))
			.arg(varOrNum(wheelDeltaX))
			.arg(varOrNum(wheelDeltaY))
			.toStdString();
	case Type::KEY_PRESS: {
		const char *dirKey =
			keyUp ? "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.keyUp"
			      : "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.keyDown";
		if (text.empty()) {
			return QString(obs_module_text(
					       "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.keyPress"))
				.arg(obs_module_text(dirKey))
				.toStdString();
		}
		return QString(obs_module_text(
				       "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.keyPressWithText"))
			.arg(obs_module_text(dirKey))
			.arg(QString::fromStdString(text))
			.toStdString();
	}
	case Type::TYPE_TEXT:
		return QString(obs_module_text(
				       "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.typeText"))
			.arg(QString::fromStdString(text))
			.toStdString();
	case Type::WAIT:
		return QString(obs_module_text(
				       "AdvSceneSwitcher.action.sourceInteraction.step.listEntry.wait"))
			.arg(varOrNum(waitMs))
			.toStdString();
	}
	return "Unknown";
}

void PerformInteractionStep(obs_source_t *source,
			    const SourceInteractionStep &step)
{
	switch (step.type) {
	case SourceInteractionStep::Type::MOUSE_MOVE: {
		obs_mouse_event e{};
		e.x = step.x;
		e.y = step.y;
		obs_source_send_mouse_move(source, &e, false);
		break;
	}
	case SourceInteractionStep::Type::MOUSE_CLICK: {
		obs_mouse_event e{};
		e.modifiers = step.modifiers;
		e.x = step.x;
		e.y = step.y;
		obs_source_send_mouse_click(source, &e, step.button,
					    step.mouseUp, step.clickCount);
		break;
	}
	case SourceInteractionStep::Type::MOUSE_WHEEL: {
		obs_mouse_event e{};
		e.modifiers = step.modifiers;
		e.x = step.x;
		e.y = step.y;
		obs_source_send_mouse_wheel(source, &e, step.wheelDeltaX,
					    step.wheelDeltaY);
		break;
	}
	case SourceInteractionStep::Type::KEY_PRESS: {
		std::string textCopy = step.text;
		obs_key_event e{};
		e.modifiers = step.modifiers;
		e.text = textCopy.data();
		e.native_vkey =
			static_cast<uint32_t>(step.nativeVkey.GetValue());
		obs_source_send_key_click(source, &e, step.keyUp);
		break;
	}
	case SourceInteractionStep::Type::TYPE_TEXT: {
		const std::string resolvedText = step.text;
		for (const char c : resolvedText) {
			std::string ch(1, c);
			obs_key_event e{};
			e.text = ch.data();
			obs_source_send_key_click(source, &e, false);
			obs_source_send_key_click(source, &e, true);
		}
		break;
	}
	case SourceInteractionStep::Type::WAIT:
		std::this_thread::sleep_for(
			std::chrono::milliseconds(step.waitMs));
		break;
	}
}

} // namespace advss
