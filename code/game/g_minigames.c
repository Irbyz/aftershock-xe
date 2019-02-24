/*
===========================================================================
Copyright (C) 2019 davidd

This file is part of AfterShock-XE source code.

AfterShock source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

AfterShock source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AfterShock source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "g_local.h"

int numwaypoints;
int touchedWaypoints[MAX_CLIENTS];
int lastTouchedNewWaypoint[MAX_CLIENTS];
int numTouchedWaypoints[MAX_CLIENTS];

static void waypointAnimStop( gentity_t *player ) {
	int		anim;

	if( player->s.weapon == WP_GAUNTLET) {
		anim = TORSO_STAND2;
	}
	else {
		anim = TORSO_STAND;
	}
	player->s.torsoAnim = ( ( player->s.torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;
}


#define	TIMER_GESTURE	(34*66+50)
static void waypointAnimStart( gentity_t *player ) {
	player->s.torsoAnim = ( ( player->s.torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | TORSO_GESTURE;
	player->nextthink = level.time + TIMER_GESTURE;
	player->think = waypointAnimStop;

	/*
	player->client->ps.events[player->client->ps.eventSequence & (MAX_PS_EVENTS-1)] = EV_TAUNT;
	player->client->ps.eventParms[player->client->ps.eventSequence & (MAX_PS_EVENTS-1)] = 0;
	player->client->ps.eventSequence++;
	*/
	//G_AddEvent(player, EV_TAUNT, 0);
}

/*
===============
Touch_MinigameWaypoint

===============
*/
void Touch_MinigameWaypoint (gentity_t *waypoint, gentity_t *ent, trace_t *trace) {
	//int			respawn;
	//qboolean	predict;
  int clientNum;
  int wpnum;
  int mask;
  clientNum = ent-g_entities;
  wpnum = waypoint->count;
  mask = 1 << (wpnum-1);
  if (touchedWaypoints[clientNum] & mask) {
      if (lastTouchedNewWaypoint[clientNum]+2000 < level.time) {
          // sendclient print already touched
          trap_SendServerCommand(clientNum, va ("secho \"Already touched waypoint %i\"", wpnum));
          // reset the timer
          lastTouchedNewWaypoint[clientNum] = level.time;
      }
      return;
  } 
  numTouchedWaypoints[clientNum]++;
  touchedWaypoints[clientNum] |= mask;
  lastTouchedNewWaypoint[clientNum] = level.time;
  trap_SendServerCommand(clientNum, va ("secho \"You found waypoint %i, that is %i of %i\"", wpnum, numTouchedWaypoints[clientNum], numwaypoints));
  waypointAnimStart(waypoint);
  // detect if round complete;
  if (touchedWaypoints[clientNum] + 1 == (1 << numwaypoints)) {
    trap_SendServerCommand(clientNum, va ("secho \"Found all %i waypoints, reseting\"", numwaypoints));
    touchedWaypoints[clientNum] = mask;
    numTouchedWaypoints[clientNum]++;

  }





}


static gentity_t *SpawnWaypointOnSpot(  gentity_t *ent, int place ) {
	gentity_t	*waypoint;
	vec3_t		vec;
	//vec3_t		f, r, u;

	waypoint = G_Spawn();
	if ( !waypoint ) {
                G_Printf( S_COLOR_RED "ERROR: out of gentities\n" );
		return NULL;
	}

	//waypoint->classname = ent->client->pers.netname;
	waypoint->classname = "minigame_waypoint";
	//waypoint->client = ent->client;
	//waypoint->s = ent->s;
	waypoint->s.eType = ET_PLAYER;		// could be ET_INVISIBLE
	waypoint->s.eFlags = 0;				// clear EF_TALK, etc
	waypoint->s.powerups = 0;			// clear powerups
	waypoint->s.loopSound = 0;			// clear lava burning
	waypoint->s.number = waypoint - g_entities; // communicate entitity number to client
	waypoint->timestamp = level.time;
	waypoint->physicsObject = qtrue;
	waypoint->physicsBounce = 0;		// don't bounce
	waypoint->s.event = 0;
	waypoint->s.pos.trType = TR_STATIONARY;
	waypoint->s.groundEntityNum = ENTITYNUM_WORLD;
	waypoint->s.legsAnim = LEGS_IDLE;
	waypoint->s.torsoAnim = TORSO_STAND;
  //waypoint->s.modelindex = MODELINDEX_QUAD;
	waypoint->s.modelindex = G_ModelIndex( "models/powerups/teleporter/tele_enter.md3" );
	if( waypoint->s.weapon == WP_NONE ) {
		waypoint->s.weapon = WP_MACHINEGUN;
	}
	if( waypoint->s.weapon == WP_GAUNTLET) {
		waypoint->s.torsoAnim = TORSO_STAND2;
	}
	waypoint->s.event = 0;
	waypoint->r.svFlags = ent->r.svFlags;
	VectorCopy (ent->r.mins, waypoint->r.mins);
	VectorCopy (ent->r.maxs, waypoint->r.maxs);
	VectorCopy (ent->r.absmin, waypoint->r.absmin);
	VectorCopy (ent->r.absmax, waypoint->r.absmax);
	//waypoint->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	//waypoint->r.contents = CONTENTS_BODY;
	//waypoint->r.ownerNum = ent->r.ownerNum;
	waypoint->takedamage = qfalse;

	//VectorSubtract( level.intermission_origin, pad->r.currentOrigin, vec );
	//vectoangles( vec, waypoint->s.apos.trBase );
	//waypoint->s.apos.trBase[PITCH] = 0;
	//waypoint->s.apos.trBase[ROLL] = 0;

	//AngleVectors( waypoint->s.apos.trBase, f, r, u );
	//VectorMA( pad->r.currentOrigin, offset[0], f, vec );
	//VectorMA( vec, offset[1], r, vec );
	//VectorMA( vec, offset[2], u, vec );

	VectorCopy (ent->r.currentOrigin, vec);
	VectorCopy (ent->s.angles, waypoint->s.angles);
	//VectorCopy (ent->r., waypoint->r.);
	//G_SetAngles( waypoint, ent->s.angles );
	G_SetOrigin( waypoint, vec );

	waypoint->count = place;
  waypoint->touch = Touch_MinigameWaypoint ;

  // makes touch work
	waypoint->r.contents = CONTENTS_TRIGGER;		// replaces the -1 from trap_SetBrushModel
	//waypoint->r.svFlags = SVF_NOCLIENT;

	trap_LinkEntity (waypoint);


	return waypoint;
}

/*
===========
Begin minigame

============
*/
void G_beginMinigame(void) {
	gentity_t	*spot;
	//gentity_t	*ent;
  int i;
  int number = 0;

	spot = NULL;
	
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL && number < 32) {
		
		if( spot->flags & FL_NO_HUMANS )
		{
			continue;
		}
    if (number > 0) {
		  //ent = SpawnWaypointOnSpot(spot,number);
		  SpawnWaypointOnSpot(spot,number);
    }
    number++;
	}
  numwaypoints=number-1;
  // clear player data
  for (i=0; i<MAX_CLIENTS;i++) {
      touchedWaypoints[i] = 0;
      lastTouchedNewWaypoint[i] = 0;
      numTouchedWaypoints[i] = 0;
  }
}

