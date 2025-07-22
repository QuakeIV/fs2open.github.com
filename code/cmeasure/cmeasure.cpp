/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 



#include "cmeasure/cmeasure.h"
#include "gamesnd/gamesnd.h"
#include "hud/hud.h"
#include "math/staticrand.h"
#include "mission/missionparse.h"
#include "network/multimsgs.h"
#include "object/object.h"
#include "ship/ship.h"
#include "weapon/weapon.h"

int  Cmeasures_homing_check = 0;
int  Countermeasures_enabled = 1;      //  Debug, set to 0 means no one can fire countermeasures.
const float CMEASURE_DETONATE_DISTANCE = 40.0f;

void cmeasure_select_next(ship *shipp)
{
  Assert(shipp != NULL);
  int i, new_index;

  for (i = 1; i < Num_weapon_types; i++)
  {
    new_index = (shipp->current_cmeasure + i) % Num_weapon_types;

    if(Weapon_info[new_index].wi_flags[Weapon::Info_Flags::Cmeasure])
    {
      shipp->current_cmeasure = new_index;
      return;
    }
  }

  mprintf(("Countermeasure type set to %i in frame %i\n", shipp->current_cmeasure, Framecount));
}


/** 
 * @brief If this is a player countermeasure, let the player know they evaded a missile.
 * @param objp [description]
 *
 * During single player games, this function notifies the player that evasion has occurred.
 * Multiplayer games ensure that #send_countermeasure_success_packet() is called to notify the other player.
 */

void cmeasure_maybe_alert_success(object *objp)
{
  //Is this a countermeasure, and does it have a parent
  if ( objp->type != OBJ_WEAPON || objp->parent < 0) {
    return;
  }

  Assert(Weapon_info[Weapons[objp->instance].weapon_info_index].wi_flags[Weapon::Info_Flags::Cmeasure]);

  if ( objp->parent == OBJ_INDEX(Player_obj) ) {
    hud_start_text_flash(XSTR("Evaded", 1430), 800);
    snd_play(&Snds[ship_get_sound(Player_obj, SND_MISSILE_EVADED_POPUP)]);
  } else if ( Objects[objp->parent].flags[Object::Object_Flags::Player_ship] ) {
    send_countermeasure_success_packet( objp->parent );
  }
}
