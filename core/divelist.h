// SPDX-License-Identifier: GPL-2.0
#ifndef DIVELIST_H
#define DIVELIST_H

#include "triptable.h"
#include "divesitetable.h"
#include "units.h"
#include <memory>
#include <vector>

struct dive;
struct divelog;
struct device;
struct deco_state;

int comp_dives(const struct dive &a, const struct dive &b);
int comp_dives_ptr(const struct dive *a, const struct dive *b);

struct dive_table : public sorted_owning_table<dive, &comp_dives> {
	dive *get_by_uniq_id(int id) const;
	void record_dive(std::unique_ptr<dive> d);	// call fixup_dive() before adding dive to table.
	struct dive *register_dive(std::unique_ptr<dive> d);
	std::unique_ptr<dive> unregister_dive(int idx);
	std::unique_ptr<dive> default_dive();		// generate a sensible looking defaultdive 1h from now.

	// Some of these functions act on dives, but need data from adjacent dives,
	// notably to calculate CNS, surface interval, etc. Therefore, they are called
	// on the dive_table and not on the dive.
	void fixup_dive(struct dive &dive) const;
	void force_fixup_dive(struct dive &d) const;
	int init_decompression(struct deco_state *ds, const struct dive *dive, bool in_planner) const;
	void update_cylinder_related_info(struct dive &dive) const;
	int get_dive_nr_at_idx(int idx) const;
	timestamp_t get_surface_interval(timestamp_t when) const;
	struct dive *find_next_visible_dive(timestamp_t when);
private:
	int calculate_cns(struct dive &dive) const; // Note: writes into dive->cns
};

/* this is used for both git and xml format */
#define DATAFORMAT_VERSION 3

extern void get_dive_gas(const struct dive *dive, int *o2_p, int *he_p, int *o2low_p);

int get_min_datafile_version();
void report_datafile_version(int version);
void clear_dive_file_data();
extern bool has_dive(unsigned int deviceid, unsigned int diveid);

#endif // DIVELIST_H
