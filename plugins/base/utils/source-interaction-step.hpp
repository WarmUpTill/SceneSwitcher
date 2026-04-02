#pragma once
#include "variable-number.hpp"
#include "variable-string.hpp"

#include <obs.hpp>
#include <obs-interaction.h>

namespace advss {

struct SourceInteractionStep {
	enum class Type {
		MOUSE_MOVE,
		MOUSE_CLICK,
		MOUSE_WHEEL,
		KEY_PRESS,
		TYPE_TEXT,
		WAIT,
	};

	Type type = Type::MOUSE_MOVE;

	// Mouse fields
	NumberVariable<int> x = 0;
	NumberVariable<int> y = 0;
	obs_mouse_button_type button = MOUSE_LEFT;
	bool mouseUp = false;
	NumberVariable<int> clickCount = 1;
	NumberVariable<int> wheelDeltaX = 0;
	NumberVariable<int> wheelDeltaY = 120;

	// Key / text fields
	NumberVariable<int> nativeVkey = 0;
	uint32_t modifiers = 0; // INTERACT_*
	bool keyUp = false;
	StringVariable text; // for TYPE_TEXT or key text

	// Wait field (ms)
	NumberVariable<int> waitMs = 100;

	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string ToString() const;
};

void PerformInteractionStep(obs_source_t *source,
			    const SourceInteractionStep &step);

} // namespace advss
