#include "g_local.h"

/*
===================
READY / NOTREADY

Sets a player's "ready" status.

Tardo - rewrote this because the parameter handling to the function is different in rtcw.
===================
*/
void G_readyHandle( gentity_t* ent, qboolean ready ) {
	ent->client->pers.ready = ready;
}

void G_ready_cmd( gentity_t *ent, qboolean state ) {
	char *status[2] = { "^zNOT READY^7", "^3READY^7" };

	if (!g_tournament.integer) {
		return;
	}

	if ( g_gamestate.integer == GS_PLAYING || g_gamestate.integer == GS_INTERMISSION ) {
		CP( "@print \"Match is already in progress!\n\"" );
		return;
	}

	if ( !state && g_gamestate.integer == GS_WARMUP_COUNTDOWN ) {
		CP( "print \"Countdown started..^znotready^7 ignored!\n\"" );
		return;
	}

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		CP( va("print \"Specs cannot use %s ^7command!\n\"", status[state] ));
		return;
	}

	//if (level.readyTeam[ent->client->sess.sessionTeam] == qtrue && !state) { // Doesn't cope with unreadyteam but it's out anyway..
	//	CP(va("print \"%s ^7ignored. Your team has issued ^3TEAM READY ^7command..\n\"", status[state]));
	//	return;
	//}

	// Move them to correct ready state
	if ( ent->client->pers.ready == state ) {
		CP( va( "print \"You are already %s!\n\"", status[state] ) );
	} else {
		ent->client->pers.ready = state;
		if ( !level.intermissiontime ) {
			if (state) {
				ent->client->pers.ready = qtrue;
				//ent->client->ps.powerups[PW_READY] = INT_MAX;
				//player_ready_status[ent->client->ps.clientNum].isReady = 1;
			}
			else {
				ent->client->pers.ready = qfalse;
				//ent->client->ps.powerups[PW_READY] = 0;
				//player_ready_status[ent->client->ps.clientNum].isReady = 0;
			}

			// Doesn't rly matter..score tab will show slow ones..
			AP( va( "cp \"\n%s \n^3is %s!\n\"", ent->client->pers.netname, status[state] ) );
		}
	}
}

/*
===================
TEAM-READY / NOTREADY
===================
*/
void pCmd_teamReady(gentity_t *ent, qboolean ready) {
	char *status[2] = { "NOT READY", "READY" };
	int i, p = { 0 };
	int team = ent->client->sess.sessionTeam;
	gentity_t *cl;

	if (!g_tournament.integer) {
		return;
	}
	if (team_nocontrols.integer) {
		CP("print \"Team commands are not enabled on this server.\n\"");
		return;
	}
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
		CP("print \"Specs cannot use ^3TEAM ^7commands.\n\"");
		return;
	}
	if (!ready && g_gamestate.integer == GS_WARMUP_COUNTDOWN) {
		CP("print \"Countdown started, ^3notready^7 ignored.\n\"");
		return;
	}

	for (i = 0; i < level.numConnectedClients; i++) {
		cl = g_entities + level.sortedClients[i];

		if (!cl->inuse) {
			continue;
		}

		if (cl->client->sess.sessionTeam != team) {
			continue;
		}

		if ((cl->client->pers.ready != ready) && !level.intermissiontime) {
			cl->client->pers.ready = ready;
			++p;
		}
	}

	if (!p) {
		CP(va("print \"Your team is already ^3%s^7!\n\"", status[ready]));
	}
	else {
		AP(va("cp \"%s ^7team is %s%s!\n\"", aTeams[team], (ready ? "^3" : "^z"), status[ready]));
		G_matchPrintInfo(va("%s ^7team is %s%s! ^7(%s)\n", aTeams[team], (ready ? "^3" : "^z"), status[ready], ent->client->pers.netname), qfalse);
		level.readyTeam[team] = ready;
	}
}

/******************* Client commands *******************/
qboolean playerCmds (gentity_t *ent, char *cmd ) {
if(!Q_stricmp(cmd, "readyteam"))			{ pCmd_teamReady(ent, qtrue);	return qtrue;}
else if(!Q_stricmp(cmd, "ready"))				{ G_ready_cmd( ent, qtrue ); return qtrue;}
	else if(!Q_stricmp(cmd, "unready") ||
			!Q_stricmp(cmd, "notready"))			{ G_ready_cmd( ent, qfalse ); return qtrue;}
else
	return qfalse;
}