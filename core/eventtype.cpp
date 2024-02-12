// SPDX-License-Identifier: GPL-2.0
#include "eventtype.h"
#include "event.h"
#include "subsurface-string.h"

#include <string>
#include <vector>
#include <algorithm>

struct event_type {
	std::string name;
	event_severity severity;
	bool hidden;
	event_type(const event *ev)
		: name(ev->name), severity(get_event_severity(ev)), hidden(false)
	{
	}
	// Waiting for C++20 space ship operator
	bool operator<(const event *t2) {
		event_severity severity2 = get_event_severity(t2);
		return std::tie(name, severity) < std::tie(t2->name, severity2);
	}
	bool operator!=(const event *t2) {
		event_severity severity2 = get_event_severity(t2);
		return std::tie(name, severity) != std::tie(t2->name, severity2);
	}
	bool operator==(const event *t2) {
		event_severity severity2 = get_event_severity(t2);
		return std::tie(name, severity) == std::tie(t2->name, severity2);
	}
};

static std::vector<event_type> event_types;

extern "C" void clear_event_types()
{
	event_types.clear();
}

extern "C" void remember_event_type(const event *ev)
{
	if (empty_string(ev->name))
		return;
	// Insert in ordered manner using binary search
	auto it = std::lower_bound(event_types.begin(), event_types.end(), ev);
	if (it == event_types.end() || *it != ev)
		event_types.insert(it, ev);
}

extern "C" void hide_event_type(const event *ev)
{
	auto it = std::lower_bound(event_types.begin(), event_types.end(), ev);
	if (it != event_types.end() && *it == ev)
		it->hidden = true;
}

extern "C" bool is_event_type_hidden(const event *ev)
{
	auto it = std::lower_bound(event_types.begin(), event_types.end(), ev);
	return it == event_types.end() || *it != ev ? false : it->hidden;
}

extern "C" void show_all_event_types()
{
	for (event_type &en: event_types)
		en.hidden = false;
}

extern "C" bool any_event_types_hidden()
{
	return std::any_of(event_types.begin(), event_types.end(),
			   [] (const event_type &en) { return en.hidden; });
}
