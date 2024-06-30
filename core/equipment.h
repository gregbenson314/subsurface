// SPDX-License-Identifier: GPL-2.0
#ifndef EQUIPMENT_H
#define EQUIPMENT_H

#include "gas.h"

#include <memory>
#include <string>
#include <vector>

struct dive;

enum cylinderuse {OC_GAS, DILUENT, OXYGEN, NOT_USED, NUM_GAS_USE}; // The different uses for cylinders
extern const char *cylinderuse_text[NUM_GAS_USE];

struct cylinder_type_t
{
	volume_t size;
	pressure_t workingpressure;
	std::string description; /* "LP85", "AL72", "AL80", "HP100+" or whatever */
};

struct cylinder_t
{
	cylinder_type_t type;
	struct gasmix gasmix = gasmix_air;
	pressure_t start, end, sample_start, sample_end;
	depth_t depth;
	bool manually_added = false;
	volume_t gas_used;
	volume_t deco_gas_used;
	enum cylinderuse cylinder_use = OC_GAS;
	bool bestmix_o2 = false;
	bool bestmix_he = false;

	cylinder_t();
	~cylinder_t();
};

/* Table of cylinders.
 * This is a crazy class: it is basically a std::vector<>, but overrides
 * the [] accessor functions and allows out-of-bound accesses.
 * This is used in the planner, which uses "max_index + 1" for the
 * surface air cylinder.
 * Note: an out-of-bound access returns a reference to an object with
 * static linkage that MUST NOT be written into.
 * Yes, this is all very mad, but it grew historically.
 */
struct cylinder_table : public std::vector<cylinder_t> {
	cylinder_t &operator[](size_t i);
	const cylinder_t &operator[](size_t i) const;
};

struct weightsystem_t
{
	weight_t weight;
	std::string description; /* "integrated", "belt", "ankle" */
	bool auto_filled = false; /* weight was automatically derived from the type */

	weightsystem_t();
	weightsystem_t(weight_t w, std::string desc, bool auto_filled);
	~weightsystem_t();

	bool operator==(const weightsystem_t &w2) const;
};

/* Table of weightsystems. Attention: this stores weightsystems,
 * *not* pointers * to weightsystems. Therefore pointers to
 * weightsystems are *not* stable.
 */
using weightsystem_table = std::vector<weightsystem_t>;

extern enum cylinderuse cylinderuse_from_text(const char *text);
extern void copy_cylinder_types(const struct dive *s, struct dive *d);
extern cylinder_t *add_empty_cylinder(struct cylinder_table *t);
extern void remove_cylinder(struct dive *dive, int idx);
extern void remove_weightsystem(struct dive *dive, int idx);
extern void set_weightsystem(struct dive *dive, int idx, weightsystem_t ws);
extern void reset_cylinders(struct dive *dive, bool track_gas);
extern int gas_volume(const cylinder_t *cyl, pressure_t p); /* Volume in mliter of a cylinder at pressure 'p' */
extern int find_best_gasmix_match(struct gasmix mix, const struct cylinder_table &cylinders);
extern void fill_default_cylinder(const struct dive *dive, cylinder_t *cyl); /* dive is needed to fill out MOD, which depends on salinity. */
extern cylinder_t default_cylinder(const struct dive *d);
extern cylinder_t create_new_manual_cylinder(const struct dive *dive); /* dive is needed to fill out MOD, which depends on salinity. */
extern void add_default_cylinder(struct dive *dive);
extern int first_hidden_cylinder(const struct dive *d);
#ifdef DEBUG_CYL
extern void dump_cylinders(struct dive *dive, bool verbose);
#endif

/* Weightsystem table functions */
extern void add_to_weightsystem_table(weightsystem_table *, int idx, weightsystem_t ws);

/* Cylinder table functions */
extern void add_cylinder(struct cylinder_table *, int idx, cylinder_t cyl);

void get_gas_string(struct gasmix gasmix, char *text, int len);
const char *gasname(struct gasmix gasmix);

struct ws_info {
	std::string name;
	weight_t weight;
};
extern std::vector<ws_info> ws_info_table;
extern weight_t get_weightsystem_weight(const std::string &name); // returns 0 if not found
extern void add_weightsystem_description(const weightsystem_t &);

struct tank_info {
	std::string name;
	int cuft, ml, psi, bar;
};

extern std::vector<tank_info> tank_info_table;
extern struct tank_info *get_tank_info(std::vector<tank_info> &table, const std::string &name);
extern void set_tank_info_data(std::vector<tank_info> &table, const std::string &name, volume_t size, pressure_t working_pressure);
extern std::pair<volume_t, pressure_t> extract_tank_info(const struct tank_info &info);
extern std::pair<volume_t, pressure_t> get_tank_info_data(const std::vector<tank_info> &table, const std::string &name);
extern void add_cylinder_description(const cylinder_type_t &);
extern void reset_tank_info_table(std::vector<tank_info> &table);

#endif // EQUIPMENT_H
