#ifndef AI_FLAGS_H
#define AI_FLAGS_H

#include "globalincs/flagset.h"

namespace AI {
	FLAG_LIST(AI_Flags) {
		Formation_wing,				//	Fly in formation as part of wing.
		Awaiting_repair,			//	Awaiting a repair ship.
		Being_repaired,				//	Currently docked with repair ship.
		Repairing,					//	Repairing a ship (or going to repair a ship)
		Seek_lock,					//	set if should focus on gaining aspect lock, not hitting with lasers
		Formation_object,			//	Fly in formation off a specific object.
		Temporary_ignore,			//	Means current ignore_objnum is only temporary, not an order from the player.
		Use_exit_path,				//  Used by path code, to flag path as an exit path
		Use_static_path,			//  Used by path code, use fixed path, don't try to recreate
		Target_collision,			//	Collided with aip->target_objnum last frame.  Avoid that ship for half a second or so.
		Unload_secondaries,			//	Fire secondaries as fast as possible!
		On_subsys_path,				//  Current path leads to a subsystem
		Avoid_shockwave_ship,		//	Avoid an existing shockwave from a ship.
		Avoid_shockwave_weapon,		//	Avoid an expected shockwave from a weapon.  shockwave_object field contains object index.
		Avoid_shockwave_started,	//	Already started avoiding shockwave, don't keep deciding whether to avoid.
		Attack_slowly,				//	Move slowly while attacking.
		Repair_obstructed,			//	Ship wants to be repaired, but path is obstructed.
		Kamikaze,					//	Crash into target
		No_dynamic,					//	Not allowed to get dynamic goals
		Avoiding_small_ship,		//	Avoiding a player ship.
		Avoiding_big_ship,			//	Avoiding a large ship.
		Big_ship_collide_recover_1,	//	Collided into a big ship.  Recovering by flying away.
		Big_ship_collide_recover_2,	//	Collided into a big ship.  Fly towards big ship sphere perimeter.
		Stealth_pursuit,			//  AI is trying to fight stealth ship
		Unload_primaries,			//	Fire primaries as fast as possible!
		Trying_unsuccessfully_to_warp,	// Trying to warp, but can't warp at the moment
		Free_afterburner_use,		// Use afterburners while following waypoints or flying towards objects

		NUM_VALUES
	};

	FLAG_LIST(Goal_Flags) {
		Docker_index_valid,	// when set, index field for docker is valid
		Dockee_index_valid,	// when set, index field for dockee is valid
		Goal_on_hold,		// when set, this goal cannot currently be satisfied, although it could be in the future
		Subsys_needs_fixup,	// when set, the subsystem index (for a destroy subsystem goal) is invalid and must be gotten from the subsys name stored in docker.name field!!
		Goal_override,		// paired with AIG_TYPE_DYNAMIC to mean this goal overrides any other goal
		Purge,				// purge this goal next time we process
		Goals_purged,		// this goal has already caused other goals to get purged
		Depart_sound_played,// Goober5000 - replacement for AL's hack ;)
		Target_own_team,

		NUM_VALUES
	};

	FLAG_LIST(Maneuver_Override_Flags) {
		Full,		//	Full sexp control
		Roll,		//	Sexp forced roll maneuver
		Pitch,		//	Sexp forced pitch change
		Heading,	//	Sexp forced heading change
		Full_lat,	//  full control over up/side/forward movement
		Up,			//	vertical movement
		Sideways,	//	horizontal movement
		Forward,	//	forward movement

		NUM_VALUES
	};

	FLAG_LIST(Profile_Flags) {
        Ai_guards_specific_ship_in_wing,
        All_ships_manage_shields,
        Allow_multi_event_scoring,
        Aspect_lock_countermeasure,
        Assist_scoring_scales_with_damage,
        Glide_decay_requires_thrust,
        Include_beams_in_stat_calcs,
        Kill_scoring_scales_with_damage,
        Multi_allow_empty_primaries,
        Multi_allow_empty_secondaries,
        Navigation_subsys_governs_warp,
        No_warp_camera,
        Perform_fewer_scream_checks,
        Use_additive_weapon_velocity,

		NUM_VALUES
	};
}

#endif
