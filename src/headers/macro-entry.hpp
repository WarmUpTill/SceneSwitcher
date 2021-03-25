#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

struct condition {
	int id;
};

struct action {
	int id;
};

class MacroEntry {

public:
	MacroEntry();
	virtual ~MacroEntry();

private:
	std::string name = "";
	std::deque<condition> conditions;
	std::deque<action> actions;
};
