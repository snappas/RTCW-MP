/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/


/*****************************************************************************
 * name:		snd_dma.c
 *
 * desc:		main control for any streaming sound output device
 *
 * $Archive: /Wolfenstein MP/src/client/snd_dma.c $
 *
 *****************************************************************************/

#include "snd_local.h"
#include "client.h"

void S_Play_f( void );
void S_SoundList_f( void );
void S_Music_f( void );
void S_StreamingSound_f( void );

void S_Update_Mix();
void S_StopAllSounds( void );
void S_UpdateStreamingSounds( void );
// Ridah, streaming sounds
// !! NOTE: the first streaming sound is always the music
streamingSound_t streamingSounds[MAX_STREAMING_SOUNDS];
int numStreamingSounds = 0;
static vec3_t entityPositions[MAX_GENTITIES];

void    *crit;

typedef struct {
	vec3_t origin;
	qboolean fixedOrigin;
	int entityNum;
	int entityChannel;
	sfxHandle_t sfx;
	int flags;
} s_pushStack;

#define MAX_PUSHSTACK 64
static s_pushStack pushPop[MAX_PUSHSTACK];
static int tart = 0;

typedef struct {
	char intro[256];
	char loop[256];
	qboolean music;
	int entnum;
	int channel;
	int attenuation;
} s_streamStack;

//static s_streamStack	Sstream[MAX_PUSHSTACK]; // TTimo: unused
//static int onStream; // TTimo: unused

// =======================================================================
// Internal sound data & structures
// =======================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define     SOUND_FULLVOLUME    80

#define     SOUND_ATTENUATE     0.0008f
#define     SOUND_RANGE_DEFAULT 1250

channel_t s_channels[MAX_CHANNELS];
channel_t loop_channels[MAX_CHANNELS];
int numLoopChannels;

static int s_soundStarted;
static qboolean s_soundMuted;
static qboolean s_soundPainted;
static int s_clearSoundBuffer = 0;
dma_t dma;

static int listener_number;
static vec3_t listener_origin;
static vec3_t listener_axis[3];

int s_soundtime;                // sample PAIRS
int s_paintedtime;              // sample PAIRS

// MAX_SFX may be larger than MAX_SOUNDS because
// of custom player sounds
#define     MAX_SFX         4096
sfx_t s_knownSfx[MAX_SFX];
int s_numSfx = 0;

#define     LOOP_HASH       128
static sfx_t       *sfxHash[LOOP_HASH];

cvar_t      *s_volume;
cvar_t      *s_testsound;
cvar_t      *s_khz;
cvar_t      *s_show;
cvar_t      *s_mixahead;
cvar_t      *s_mixPreStep;
cvar_t      *s_musicVolume;
cvar_t      *s_separation;
cvar_t      *s_doppler;
cvar_t      *s_mute;        // (SA) for DM so he can 'toggle' sound on/off without disturbing volume levels
cvar_t      *s_defaultsound; // (SA) added to silence the default beep sound if desired
cvar_t      *cl_cacheGathering; // Ridah
cvar_t      *s_wavonly;

#define MAX_LOOP_SOUNDS     128
static int numLoopSounds;
static loopSound_t loopSounds[MAX_LOOP_SOUNDS];

static channel_t       *freelist = NULL;
static channel_t       *endflist = NULL;

// Rafael
cvar_t      *s_nocompressed;

// for streaming sounds
int s_rawend[MAX_STREAMING_SOUNDS];
int s_rawpainted[MAX_STREAMING_SOUNDS];
portable_samplepair_t s_rawsamples[MAX_STREAMING_SOUNDS][MAX_RAW_SAMPLES];
// RF, store the volumes, since now they get adjusted at time of painting, so we can extract talking data first
portable_samplepair_t s_rawVolume[MAX_STREAMING_SOUNDS];


/*
================
S_SoundInfo_f
================
*/
void S_SoundInfo_f( void ) {
	Com_Printf( "----- Sound Info -----\n" );
	if ( !s_soundStarted ) {
		Com_Printf( "sound system not started\n" );
	} else {
		if ( s_soundMuted ) {
			Com_Printf( "sound system is muted\n" );
		}

		Com_Printf( "%5d stereo\n", dma.channels - 1 );
		Com_Printf( "%5d samples\n", dma.samples );
		Com_Printf( "%5d samplebits\n", dma.samplebits );
		Com_Printf( "%5d submission_chunk\n", dma.submission_chunk );
		Com_Printf( "%5d speed\n", dma.speed );
		Com_Printf( "0x%x dma buffer\n", dma.buffer );
		if ( streamingSounds[0].file ) {
			Com_Printf( "Background file: %s\n", streamingSounds[0].loop );
		} else {
			Com_Printf( "No background file.\n" );
		}

	}
	Com_Printf( "----------------------\n" );
}

void S_ChannelSetup();

/*
================
S_Init
================
*/
void S_Init( void ) {
	cvar_t  *cv;
	qboolean r;

	Com_Printf( "\n------- sound initialization -------\n" );

	s_mute = Cvar_Get( "s_mute", "0", CVAR_TEMP ); //----(SA)	added
	s_volume = Cvar_Get( "s_volume", "0.8", CVAR_ARCHIVE );
	s_musicVolume = Cvar_Get( "s_musicvolume", "0.25", CVAR_ARCHIVE );
	s_separation = Cvar_Get( "s_separation", "0.5", CVAR_ARCHIVE );
	s_doppler = Cvar_Get( "s_doppler", "1", CVAR_ARCHIVE );
	s_khz = Cvar_Get( "s_khz", "22", CVAR_ARCHIVE );
	s_mixahead = Cvar_Get( "s_mixahead", "0.2", CVAR_ARCHIVE );

	s_mixPreStep = Cvar_Get( "s_mixPreStep", "0.05", CVAR_ARCHIVE );
	s_show = Cvar_Get( "s_show", "0", CVAR_CHEAT );
	s_testsound = Cvar_Get( "s_testsound", "0", CVAR_CHEAT );
	s_defaultsound = Cvar_Get( "s_defaultsound", "0", CVAR_ARCHIVE );
	s_wavonly = Cvar_Get( "s_wavonly", "0", CVAR_ARCHIVE | CVAR_LATCH );
	// Ridah
	cl_cacheGathering = Cvar_Get( "cl_cacheGathering", "0", 0 );

	// Rafael
	s_nocompressed = Cvar_Get( "s_nocompressed", "0", CVAR_INIT );

	cv = Cvar_Get( "s_initsound", "1", 0 );
	if ( !cv->integer ) {
		Com_Printf( "not initializing.\n" );
		Com_Printf( "------------------------------------\n" );
		return;
	}

	crit = Sys_InitializeCriticalSection();

	Cmd_AddCommand( "play", S_Play_f );
	Cmd_AddCommand( "music", S_Music_f );
	Cmd_AddCommand( "streamingsound", S_StreamingSound_f );
	Cmd_AddCommand( "s_list", S_SoundList_f );
	Cmd_AddCommand( "s_info", S_SoundInfo_f );
	Cmd_AddCommand( "s_stop", S_StopAllSounds );

	r = SNDDMA_Init();
	Com_Printf( "------------------------------------\n" );

	if ( r ) {
		Com_Memset( sfxHash, 0, sizeof( sfx_t * ) * LOOP_HASH );

		s_soundStarted = 1;
		s_soundMuted = 1;
//		s_numSfx = 0;

		s_soundtime = 0;
		s_paintedtime = 0;

		S_StopAllSounds();

		S_SoundInfo_f();
		S_ChannelSetup();
	}

}

/*
================
S_ChannelFree
================
*/
void S_ChannelFree( channel_t *v ) {
	v->thesfx = NULL;
	v->threadReady = qfalse;
	*(channel_t **)endflist = v;
	endflist = v;
	*(channel_t **)v = NULL;
}

/*
================
S_ChannelMalloc
================
*/
channel_t*  S_ChannelMalloc() {
	channel_t *v;
	if ( freelist == NULL ) {
		return NULL;
	}
	v = freelist;
	freelist = *(channel_t **)freelist;
	v->allocTime = Sys_Milliseconds();
	return v;
}

/*
================
S_ChannelSetup
================
*/
void S_ChannelSetup() {
	channel_t *p, *q;

	// clear all the sounds so they don't
	Com_Memset( s_channels, 0, sizeof( s_channels ) );

	p = s_channels;;
	q = p + MAX_CHANNELS;
	while ( --q > p ) {
		*(channel_t **)q = q - 1;
	}

	endflist = q;
	*(channel_t **)q = NULL;
	freelist = p + MAX_CHANNELS - 1;
	Com_DPrintf( "Channel memory manager started\n" );
}

/*
================
S_Shutdown
================
*/
void S_Shutdown( void ) {
	if ( !s_soundStarted ) {
		return;
	}

	Sys_EnterCriticalSection( crit );

	SNDDMA_Shutdown();

	s_soundStarted = 0;
	s_soundMuted = qtrue;

	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "stopsound" );
	Cmd_RemoveCommand( "soundlist" );
	Cmd_RemoveCommand( "soundinfo" );
}

/*
================
S_HashSFXName

return a hash value for the sfx name
================
*/
static long S_HashSFXName( const char *name ) {
	int i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while ( name[i] != '\0' ) {
		letter = tolower( name[i] );
		if ( letter == '.' ) {
			break;                          // don't include extension
		}
		if ( letter == '\\' ) {
			letter = '/';                   // damn path names
		}
		hash += (long)( letter ) * ( i + 119 );
		i++;
	}
	hash &= ( LOOP_HASH - 1 );
	return hash;
}

/*
======================
S_FreeOldestSound
======================
*/
void S_FreeOldestSound( void ) {
	int i, oldest, used;
	sfx_t   *sfx;
	sndBuffer   *buffer, *nbuffer;

	oldest = Sys_Milliseconds();
	used = 0;

	for ( i = 1 ; i < s_numSfx ; i++ ) {
		sfx = &s_knownSfx[i];
		if ( sfx->inMemory && sfx->lastTimeUsed < oldest ) {
			used = i;
			oldest = sfx->lastTimeUsed;
		}
	}

	sfx = &s_knownSfx[used];

	// DHM - Nerve :: can cause race conditions
	//Com_DPrintf("S_FreeOldestSound: freeing sound %s\n", sfx->soundName);

	buffer = sfx->soundData;
	while ( buffer != NULL ) {
		nbuffer = buffer->next;
		SND_free( buffer );
		buffer = nbuffer;
	}
	sfx->inMemory = qfalse;
	sfx->soundData = NULL;
}

/*
==================
S_FindName

Will allocate a new sfx if it isn't found
==================
*/
static sfx_t *S_FindName( const char *name ) {
	int i;
	int hash;

	sfx_t   *sfx;

	if ( !name ) {
		//Com_Error (ERR_FATAL, "S_FindName: NULL\n");
		name = "*default*";
	}
	if ( !name[0] ) {
		//Com_Error (ERR_FATAL, "S_FindName: empty name\n");
		name = "*default*";
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Error( ERR_FATAL, "Sound name too long: %s", name );
	}

	hash = S_HashSFXName( name );

	sfx = sfxHash[hash];
	// see if already loaded
	while ( sfx ) {
		if ( !Q_stricmp( sfx->soundName, name ) ) {
			return sfx;
		}
		sfx = sfx->next;
	}

	// find a free sfx
	for ( i = 0 ; i < s_numSfx ; i++ ) {
		if ( !s_knownSfx[i].soundName[0] ) {
			break;
		}
	}

	if ( i == s_numSfx ) {
		if ( s_numSfx == MAX_SFX ) {
			Com_Error( ERR_FATAL, "S_FindName: out of sfx_t" );
		}
		s_numSfx++;
	}

	sfx = &s_knownSfx[i];
	Com_Memset( sfx, 0, sizeof( *sfx ) );
	strcpy( sfx->soundName, name );

	sfx->next = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}

/*
=================
S_DefaultSound
=================
*/
void S_DefaultSound( sfx_t *sfx ) {
	int i;

	sfx->soundLength = 512;
	sfx->soundData = SND_malloc();
	sfx->soundData->next = NULL;

	if ( s_defaultsound->integer ) {
		for ( i = 0 ; i < sfx->soundLength ; i++ ) {
			sfx->soundData->sndChunk[i] = i;
		}
	} else {
		for ( i = 0 ; i < sfx->soundLength ; i++ ) {
			sfx->soundData->sndChunk[i] = 0;
		}
	}
}

/*
===================
S_DisableSounds

Disables sounds until the next S_BeginRegistration.
This is called when the hunk is cleared and the sounds
are no longer valid.
===================
*/
void S_DisableSounds( void ) {
	S_StopAllSounds();
	s_soundMuted = qtrue;
}

/*
=====================
S_BeginRegistration
=====================
*/
void S_BeginRegistration( void ) {
	sfx_t   *sfx;
	s_soundMuted = qfalse;      // we can play again

	if ( s_numSfx == 0 ) {
		SND_setup();

		s_numSfx = 0;
		Com_Memset( s_knownSfx, 0, sizeof( s_knownSfx ) );
		Com_Memset( sfxHash, 0, sizeof( sfx_t * ) * LOOP_HASH );

		sfx = S_FindName( "***DEFAULT***" );
		S_DefaultSound( sfx );
	}
}

/*
==================
S_RegisterSound

Creates a default buzz sound if the file can't be loaded
==================
*/
sfxHandle_t S_RegisterSound( const char *name, qboolean compressed ) {
	sfx_t   *sfx;

	if ( !s_soundStarted ) {
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_DPrintf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}

	sfx = S_FindName( name );
	if ( sfx->soundData ) {
		if ( sfx->defaultSound ) {
			if ( com_developer->integer ) {
				Com_DPrintf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
			}
			return 0;
		}
		return sfx - s_knownSfx;
	}

	sfx->inMemory = qfalse;
	sfx->soundCompressed = compressed;

//	if (!compressed) {
	S_memoryLoad( sfx );
//	}

	if ( sfx->defaultSound ) {
		if ( com_developer->integer ) {
			Com_DPrintf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
		}
		return 0;
	}

	return sfx - s_knownSfx;
}

/*
=================
S_memoryLoad
=================
*/
void S_memoryLoad( sfx_t *sfx ) {
	// load the sound file
	if ( !S_LoadSound( sfx ) ) {
//		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't load sound: %s\n", sfx->soundName );
		sfx->defaultSound = qtrue;
	}
	sfx->inMemory = qtrue;
}

//=============================================================================

/*
=================
S_SpatializeOrigin

Used for spatializing s_channels
=================
*/
void S_SpatializeOrigin( vec3_t origin, int master_vol, int *left_vol, int *right_vol, float range ) {
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;
	vec3_t vec;

//	const float dist_mult = SOUND_ATTENUATE;
	float dist_mult, dist_fullvol;

	dist_fullvol = range * 0.064f;        // default range of 1250 gives 80
	dist_mult = dist_fullvol * 0.00001f;  // default range of 1250 gives .0008
//	dist_mult = range*0.00000064f;		// default range of 1250 gives .0008

	// calculate stereo seperation and distance attenuation
	VectorSubtract( origin, listener_origin, source_vec );

	dist = VectorNormalize( source_vec );
//	dist -= SOUND_FULLVOLUME;
	dist -= dist_fullvol;
	if ( dist < 0 ) {
		dist = 0;           // close enough to be at full volume

	}
	if ( dist ) {
		dist = dist / range;  // FIXME: lose the divide again
	}
//	dist *= dist_mult;		// different attenuation levels

	VectorRotate( source_vec, listener_axis, vec );

	dot = -vec[1];

	if ( dma.channels == 1 ) { // no attenuation = no spatialization
		rscale = 1.0;
		lscale = 1.0;
	} else
	{
		rscale = 0.5 * ( 1.0 + dot );
		lscale = 0.5 * ( 1.0 - dot );
		//rscale = s_separation->value + ( 1.0 - s_separation->value ) * dot;
		//lscale = s_separation->value - ( 1.0 - s_separation->value ) * dot;
		if ( rscale < 0 ) {
			rscale = 0;
		}
		if ( lscale < 0 ) {
			lscale = 0;
		}
	}

	// add in distance effect
	scale = ( 1.0 - dist ) * rscale;
	*right_vol = ( master_vol * scale );
	if ( *right_vol < 0 ) {
		*right_vol = 0;
	}

	scale = ( 1.0 - dist ) * lscale;
	*left_vol = ( master_vol * scale );
	if ( *left_vol < 0 ) {
		*left_vol = 0;
	}
}

/*
====================
S_StartSound

Validates the parms and queues the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound

  flags:  (currently apply only to non-looping sounds)
	SND_NORMAL			    0	- (default) allow sound to be cut off only by the same sound on this channel
	SND_OKTOCUT			0x001	- allow sound to be cut off by any following sounds on this channel
	SND_REQUESTCUT		0x002	- allow sound to be cut off by following sounds on this channel only for sounds who request cutoff
	SND_CUTOFF			0x004	- cut off sounds on this channel that are marked 'SND_REQUESTCUT'
	SND_CUTOFF_ALL		0x008	- cut off all sounds on this channel
====================
*/

void S_StartSoundEx( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, int flags ) {
	if ( !s_soundStarted || s_soundMuted || ( cls.state != CA_ACTIVE && cls.state != CA_DISCONNECTED ) ) {
		return;
	}
	if ( tart < MAX_PUSHSTACK ) {
		sfx_t       *sfx;
		if ( origin ) {
			VectorCopy( origin, pushPop[tart].origin );
			pushPop[tart].fixedOrigin = qtrue;
		} else {
			pushPop[tart].fixedOrigin = qfalse;
		}
		pushPop[tart].entityNum = entityNum;
		pushPop[tart].entityChannel = entchannel;
		pushPop[tart].sfx = sfxHandle;
		pushPop[tart].flags = flags;
		sfx = &s_knownSfx[ sfxHandle ];

		if ( sfx->inMemory == qfalse ) {
			S_memoryLoad( sfx );
		}

		tart++;
	}
}

void S_ThreadStartSoundEx( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, int flags ) {
	channel_t   *ch;
	sfx_t       *sfx;
	int i, oldest, chosen;

	chosen = -1;
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( !origin && ( entityNum < 0 || entityNum > MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_DPrintf( S_COLOR_YELLOW, "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if ( s_show->integer == 1 ) {
		Com_Printf( "%i : %s\n", s_paintedtime, sfx->soundName );
	}

//	Com_Printf("playing %s\n", sfx->soundName);

	sfx->lastTimeUsed = Sys_Milliseconds();

	// check for a streaming sound that this entity is playing in this channel
	// kill it if it exists
	if ( entityNum >= 0 ) {
		for ( i = 1; i < MAX_STREAMING_SOUNDS; i++ ) {    // track 0 is music/cinematics
			if ( !streamingSounds[i].file ) {
				continue;
			}
			// check to see if this character currently has another sound streaming on the same channel
			if ( ( entchannel != CHAN_AUTO ) && ( streamingSounds[i].entnum >= 0 ) && ( streamingSounds[i].channel == entchannel ) && ( streamingSounds[i].entnum == entityNum ) ) {
				// found a match, override this channel
				streamingSounds[i].kill = qtrue;
				break;
			}
		}
	}

	ch = NULL;

//----(SA)	modified

	// shut off other sounds on this channel if necessary
	for ( i = 0 ; i < MAX_CHANNELS ; i++ ) {
		if ( s_channels[i].entnum == entityNum && s_channels[i].thesfx && s_channels[i].entchannel == entchannel ) {

			// cutoff all on channel
			if ( flags & SND_CUTOFF_ALL ) {
				S_ChannelFree( &s_channels[i] );
				continue;
			}

			if ( s_channels[i].flags & SND_NOCUT ) {
				continue;
			}

			// cutoff sounds that expect to be overwritten
			if ( s_channels[i].flags & SND_OKTOCUT ) {
				S_ChannelFree( &s_channels[i] );
				continue;
			}

			// cutoff 'weak' sounds on channel
			if ( flags & SND_CUTOFF ) {
				if ( s_channels[i].flags & SND_REQUESTCUT ) {
					S_ChannelFree( &s_channels[i] );
					continue;
				}
			}

		}
	}

	// re-use channel if applicable
	for ( i = 0 ; i < MAX_CHANNELS ; i++ ) {
		if ( s_channels[i].entnum == entityNum && s_channels[i].entchannel == entchannel ) {
			if ( !( s_channels[i].flags & SND_NOCUT ) && s_channels[i].thesfx == sfx ) {
				ch = &s_channels[i];
				break;
			}
		}
	}

	if ( !ch ) {
		ch = S_ChannelMalloc();
	}
//----(SA)	end

	if ( !ch ) {
		ch = s_channels;

		oldest = sfx->lastTimeUsed;
		for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
			if ( ch->entnum == entityNum && ch->thesfx == sfx ) {
				chosen = i;
				break;
			}
			if ( ch->entnum != listener_number && ch->entnum == entityNum && ch->allocTime < oldest && ch->entchannel != CHAN_ANNOUNCER ) {
				oldest = ch->allocTime;
				chosen = i;
			}
		}
		if ( chosen == -1 ) {
			ch = s_channels;
			for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
				if ( ch->entnum != listener_number && ch->allocTime < oldest && ch->entchannel != CHAN_ANNOUNCER ) {
					oldest = ch->allocTime;
					chosen = i;
				}
			}
			if ( chosen == -1 ) {
				if ( ch->entnum == listener_number ) {
					for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
						if ( ch->allocTime < oldest ) {
							oldest = ch->allocTime;
							chosen = i;
						}
					}
				}
				if ( chosen == -1 ) {
					//Com_Printf("dropping sound\n");
					return;
				}
			}
		}
		ch = &s_channels[chosen];
		ch->allocTime = sfx->lastTimeUsed;
	}

	if ( origin ) {
		VectorCopy( origin, ch->origin );
		ch->fixed_origin = qtrue;
	} else {
		ch->fixed_origin = qfalse;
	}

	ch->flags = flags;  //----(SA)	added
	ch->master_vol = 127;
	ch->entnum = entityNum;
	ch->thesfx = sfx;
	ch->entchannel = entchannel;
	ch->leftvol = ch->master_vol;       // these will get calced at next spatialize
	ch->rightvol = ch->master_vol;      // unless the game isn't running
	ch->doppler = qfalse;

	if ( ch->fixed_origin ) {
		S_SpatializeOrigin( ch->origin, ch->master_vol, &ch->leftvol, &ch->rightvol, SOUND_RANGE_DEFAULT );
	} else {
		S_SpatializeOrigin( entityPositions[ ch->entnum ], ch->master_vol, &ch->leftvol, &ch->rightvol, SOUND_RANGE_DEFAULT );
	}

	ch->startSample = START_SAMPLE_IMMEDIATE;
	ch->threadReady = qtrue;
}

/*
==============
S_StartSound
==============
*/
void S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle ) {
	S_StartSoundEx( origin, entityNum, entchannel, sfxHandle, 0 );
}



/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound( sfxHandle_t sfxHandle, int channelNum ) {
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_DPrintf( S_COLOR_YELLOW, "S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return;
	}

	S_StartSound( NULL, listener_number, channelNum, sfxHandle );
}


/*
==================
S_ClearSoundBuffer

If we are about to perform file access, clear the buffer
so sound doesn't stutter.
==================
*/
void S_ClearSoundBuffer( void ) {
	if ( !s_soundStarted ) {
		return;
	}

	if ( !s_soundPainted ) {  // RF, buffers are clear, no point clearing again
		return;
	}

	s_soundPainted = qfalse;

	s_clearSoundBuffer = 4;

	S_Update();         // NERVE - SMF - force an update
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds( void ) {
	int i;
	if ( !s_soundStarted ) {
		return;
	}

//DAJ BUGFIX	for(i=0;i<numStreamingSounds;i++) {
	for ( i = 0; i < MAX_STREAMING_SOUNDS; i++ ) {   //DAJ numStreamingSounds can get bigger than the MAX array size
		streamingSounds[i].kill = qtrue;
	}

	// stop the background music
	S_StopBackgroundTrack();

	S_ClearSoundBuffer();
}

/*
==============================================================

continuous looping sounds are added each frame

==============================================================
*/

/*
==================
S_ClearLoopingSounds

==================
*/
void S_ClearLoopingSounds( void ) {
	numLoopSounds = 0;
}

/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...

NOTE: 'volume' with underwater bit set stays at set volume underwater
==================
*/

#define UNDERWATER_BIT  8

void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, const int range, sfxHandle_t sfxHandle, int volume ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted || cls.state != CA_ACTIVE ) {
		return;
	}
	if ( numLoopSounds >= MAX_LOOP_SOUNDS ) {
		return;
	}
	if ( !volume ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Error( ERR_DROP, "S_AddLoopingSound: handle %i out of range", sfxHandle );
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if ( sfx->inMemory == qfalse ) {
		S_memoryLoad( sfx );
	}

	if ( !sfx->soundLength ) {
		Com_Error( ERR_DROP, "%s has length 0", sfx->soundName );
	}
	VectorCopy( origin, loopSounds[numLoopSounds].origin );
	VectorCopy( velocity, loopSounds[numLoopSounds].velocity );
	loopSounds[numLoopSounds].sfx = sfx;
	if ( range ) {
		loopSounds[numLoopSounds].range = range;
	} else {
		loopSounds[numLoopSounds].range = SOUND_RANGE_DEFAULT;
	}

	if ( volume & 1 << UNDERWATER_BIT ) {
		loopSounds[numLoopSounds].loudUnderWater = qtrue;
	}

	if ( volume > 255 ) {
		volume = 255;
	} else if ( volume < 0 ) {
		volume = 0;
	}
	loopSounds[numLoopSounds].vol = volume;

	numLoopSounds++;
}

/*
==================
S_AddLoopSounds

Spatialize all of the looping sounds.
All sounds are on the same cycle, so any duplicates can just
sum up the channel multipliers.
==================
*/
void S_AddLoopSounds( void ) {
	int i, j, time;
	int left_total, right_total, left, right;
	channel_t   *ch;
	loopSound_t *loop, *loop2;
	static int loopFrame;

//	Sys_EnterCriticalSection(crit);

	numLoopChannels = 0;

	time = Sys_Milliseconds();

	loopFrame++;
	for ( i = 0 ; i < numLoopSounds ; i++ ) {
		loop = &loopSounds[i];
		if ( loop->mergeFrame == loopFrame ) {
			continue;   // already merged into an earlier sound
		}

		//if (loop->kill) {
		//	S_SpatializeOrigin( loop->origin, 127, &left_total, &right_total, loop->range);	// 3d
		//} else {
		S_SpatializeOrigin( loop->origin, 90,  &left_total, &right_total, loop->range );    // sphere
		//}

		// adjust according to volume
		left_total = (int)( (float)loop->vol * (float)left_total / 256.0 );
		right_total = (int)( (float)loop->vol * (float)right_total / 256.0 );

		loop->sfx->lastTimeUsed = time;

		for ( j = ( i + 1 ); j < numLoopChannels ; j++ ) {
			loop2 = &loopSounds[j];
			if ( loop2->sfx != loop->sfx ) {
				continue;
			}
			loop2->mergeFrame = loopFrame;

			//if (loop2->kill) {
			//	S_SpatializeOrigin( loop2->origin, 127, &left, &right, loop2->range);	// 3d
			//} else {
			S_SpatializeOrigin( loop2->origin, 90,  &left, &right, loop2->range );      // sphere
			//}

			// adjust according to volume
			left = (int)( (float)loop2->vol * (float)left / 256.0 );
			right = (int)( (float)loop2->vol * (float)right / 256.0 );

			loop2->sfx->lastTimeUsed = time;
			left_total += left;
			right_total += right;
		}
		if ( left_total == 0 && right_total == 0 ) {
			continue;       // not audible
		}

		// allocate a channel
		ch = &loop_channels[numLoopChannels];

		if ( left_total > 255 ) {
			left_total = 255;
		}
		if ( right_total > 255 ) {
			right_total = 255;
		}

		ch->master_vol = 127;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->thesfx = loop->sfx;

		// RF, disabled doppler for looping sounds for now, since we are reverting to the old looping sound code
		ch->doppler = qfalse;

		numLoopChannels++;
		if ( numLoopChannels == MAX_CHANNELS ) {
			i = numLoopSounds + 1;
		}
	}
//	Sys_LeaveCriticalSection(crit);
}

//=============================================================================

/*
=================
S_ByteSwapRawSamples

If raw data has been loaded in little endien binary form, this must be done.
If raw data was calculated, as with ADPCM, this should not be called.
=================
*/
//DAJ void S_ByteSwapRawSamples( int samples, int width, int s_channels, const byte *data ) {
void S_ByteSwapRawSamples( int samples, int width, int s_channels, short *data ) {
	int i;

	if ( width != 2 ) {
		return;
	}
#ifndef __MACOS__   //DAJ save this test
	if ( LittleShort( 256 ) == 256 ) {
		return;
	}
#endif
	//DAJ use a faster loop technique
	if ( s_channels == 2 ) {
		i = samples << 1;
	} else {
		i = samples;
	}

	do {
		*data = LittleShort( *data );
		data++;
//DAJ		((short *)data)[i] = LittleShort( ((short *)data)[i] );
	} while ( --i );
}

/*
============
S_GetRawSamplePointer
============
*/
portable_samplepair_t *S_GetRawSamplePointer() {
	return s_rawsamples[0];
}

/*
============
S_RawSamples

Music streaming
============
*/
void S_RawSamples( int samples, int rate, int width, int s_channels, const byte *data, float lvol, float rvol, int streamingIndex ) {
	int i;
	int src, dst;
	float scale;
	int intVolumeL, intVolumeR;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	// volume taken into account when mixed
	s_rawVolume[streamingIndex].left = 256 * lvol;
	s_rawVolume[streamingIndex].right = 256 * rvol;

	intVolumeL = 256;
	intVolumeR = 256;

	if ( s_rawend[streamingIndex] < s_soundtime ) {
		Com_DPrintf( "S_RawSamples: resetting minimum: %i < %i\n", s_rawend[streamingIndex], s_soundtime );
		s_rawend[streamingIndex] = s_soundtime;
	}

	scale = (float)rate / dma.speed;

	//Com_Printf ("%i < %i < %i\n", s_soundtime, s_paintedtime, s_rawend);
	if ( s_channels == 2 && width == 2 ) {
		if ( scale == 1.0 ) { // optimized case
			for ( i = 0; i < samples; i++ )
			{
				dst = s_rawend[streamingIndex] & ( MAX_RAW_SAMPLES - 1 );
				s_rawend[streamingIndex]++;
				s_rawsamples[streamingIndex][dst].left = ( (short *)data )[i * 2] * intVolumeL;
				s_rawsamples[streamingIndex][dst].right = ( (short *)data )[i * 2 + 1] * intVolumeR;
			}
		} else
		{
			for ( i = 0; ; i++ )
			{
				src = i * scale;
				if ( src >= samples ) {
					break;
				}
				dst = s_rawend[streamingIndex] & ( MAX_RAW_SAMPLES - 1 );
				s_rawend[streamingIndex]++;
				s_rawsamples[streamingIndex][dst].left = ( (short *)data )[src * 2] * intVolumeL;
				s_rawsamples[streamingIndex][dst].right = ( (short *)data )[src * 2 + 1] * intVolumeR;
			}
		}
	} else if ( s_channels == 1 && width == 2 )     {
		for ( i = 0; ; i++ )
		{
			src = i * scale;
			if ( src >= samples ) {
				break;
			}
			dst = s_rawend[streamingIndex] & ( MAX_RAW_SAMPLES - 1 );
			s_rawend[streamingIndex]++;
			s_rawsamples[streamingIndex][dst].left = ( (short *)data )[src] * intVolumeL;
			s_rawsamples[streamingIndex][dst].right = ( (short *)data )[src] * intVolumeR;
		}
	} else if ( s_channels == 2 && width == 1 )     {
		intVolumeL *= 256;
		intVolumeR *= 256;

		for ( i = 0 ; ; i++ )
		{
			src = i * scale;
			if ( src >= samples ) {
				break;
			}
			dst = s_rawend[streamingIndex] & ( MAX_RAW_SAMPLES - 1 );
			s_rawend[streamingIndex]++;
			s_rawsamples[streamingIndex][dst].left = ( (char *)data )[src * 2] * intVolumeL;
			s_rawsamples[streamingIndex][dst].right = ( (char *)data )[src * 2 + 1] * intVolumeR;
		}
	} else if ( s_channels == 1 && width == 1 )     {
		intVolumeL *= 256;
		intVolumeR *= 256;

		for ( i = 0; ; i++ )
		{
			src = i * scale;
			if ( src >= samples ) {
				break;
			}
			dst = s_rawend[streamingIndex] & ( MAX_RAW_SAMPLES - 1 );
			s_rawend[streamingIndex]++;
			s_rawsamples[streamingIndex][dst].left = ( ( (byte *)data )[src] - 128 ) * intVolumeL;
			s_rawsamples[streamingIndex][dst].right = ( ( (byte *)data )[src] - 128 ) * intVolumeR;
		}
	}

	if ( s_rawend[streamingIndex] > s_soundtime + MAX_RAW_SAMPLES ) {
		Com_DPrintf( "S_RawSamples: overflowed %i > %i\n", s_rawend[streamingIndex], s_soundtime );
	}
}

//=============================================================================

/*
=====================
S_UpdateEntityPosition

let the sound system know where an entity currently is
======================
*/
void S_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	if ( entityNum < 0 || entityNum > MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}
	VectorCopy( origin, entityPositions[entityNum] );
}


/*
============
S_Respatialize

Change the volumes of all the playing sounds for changes in their positions
============
*/
void S_Respatialize( int entityNum, const vec3_t head, vec3_t axis[3], int inwater ) {

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	listener_number = entityNum;
	VectorCopy( head, listener_origin );
	VectorCopy( axis[0], listener_axis[0] );
	VectorCopy( axis[1], listener_axis[1] );
	VectorCopy( axis[2], listener_axis[2] );
}

void S_ThreadRespatialize() {
	int i;
	channel_t   *ch;
	vec3_t origin;
	// update spatialization for dynamic sounds
	ch = s_channels;
	for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		// anything coming from the view entity will always be full volume
		if ( ch->entnum == listener_number ) {
			ch->leftvol = ch->master_vol;
			ch->rightvol = ch->master_vol;
		} else {
			if ( ch->fixed_origin ) {
				VectorCopy( ch->origin, origin );
			} else {
				VectorCopy( entityPositions[ ch->entnum ], origin );
			}

			S_SpatializeOrigin( origin, ch->master_vol, &ch->leftvol, &ch->rightvol, SOUND_RANGE_DEFAULT );
		}
	}
}

/*
========================
S_ScanChannelStarts

Returns qtrue if any new sounds were started since the last mix
========================
*/
qboolean S_ScanChannelStarts( void ) {
	channel_t       *ch;
	int i;
	qboolean newSamples;

	newSamples = qfalse;
	ch = s_channels;

	for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		// if this channel was just started this frame,
		// set the sample count to it begins mixing
		// into the very first sample
		if ( ch->startSample == START_SAMPLE_IMMEDIATE && ch->threadReady == qtrue ) {
			ch->startSample = s_paintedtime;
			newSamples = qtrue;
			continue;
		}

		// if it is completely finished by now, clear it
		if ( ch->startSample + ( ch->thesfx->soundLength ) <= s_paintedtime ) {
			S_ChannelFree( ch );
		}
	}

	return newSamples;
}

/*
============
S_Update

Called once each time through the main loop
============
*/

void S_Update( void ) {
	int i;
	int total;
	channel_t   *ch;

	if ( !s_soundStarted || s_soundMuted ) {
//		Com_DPrintf ("not started or muted\n");
		return;
	}

	//
	// debugging output
	//
	if ( s_show->integer == 2 ) {
		total = 0;
		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			if ( ch->thesfx && ( ch->leftvol || ch->rightvol ) ) {
				Com_DPrintf( "%i %i %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName );         // <- this is not thread safe
				total++;
			}
		}

		Com_Printf( "----(%i)---- painted: %i\n", total, s_paintedtime );
	}
	// add loopsounds
	S_AddLoopSounds();
	// do all the rest
	S_UpdateThread();
}

void S_UpdateThread( void ) {
	if ( !s_soundStarted || s_soundMuted ) {
//		Com_DPrintf ("not started or muted\n");
		return;
	}

#ifdef TALKANIM
	// default to ZERO amplitude, overwrite if sound is playing
	memset( s_entityTalkAmplitude, 0, sizeof( s_entityTalkAmplitude ) );
#endif

	if ( s_clearSoundBuffer ) {
		int clear;
		int i;
		Sys_EnterCriticalSection( crit );
		// stop looping sounds
		S_ClearLoopingSounds();

		for ( i = 0; i < MAX_STREAMING_SOUNDS; i++ ) {
			s_rawend[i] = 0;
		}

		if ( dma.samplebits == 8 ) {
			clear = 0x80;
		} else {
			clear = 0;
		}

		SNDDMA_BeginPainting();
		if ( dma.buffer ) {
			// TTimo: due to a particular bug workaround in linux sound code,
			//   have to optionally use a custom C implementation of Com_Memset
			//   not affecting win32, we have #define Snd_Memset Com_Memset
			// show_bug.cgi?id=371
			Snd_Memset( dma.buffer, clear, dma.samples * dma.samplebits / 8 );
		}
		SNDDMA_Submit();
		s_clearSoundBuffer = 0;
		Sys_LeaveCriticalSection( crit );

		// NERVE - SMF - clear out channels so they don't finish playing when audio restarts
		S_ChannelSetup();
	} else {
		Sys_EnterCriticalSection( crit );

		S_ThreadRespatialize();
		// add raw data from streamed samples
		S_UpdateStreamingSounds();
		// mix some sound
		S_Update_Mix();

		Sys_LeaveCriticalSection( crit );
	}
}
/*
============
S_GetSoundtime
============
*/
void S_GetSoundtime( void ) {
	int samplepos;
	static int buffers;
	static int oldsamplepos;
	int fullsamples;

	fullsamples = dma.samples / dma.channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();
	if ( samplepos < oldsamplepos ) {
		buffers++;                  // buffer wrapped

		if ( s_paintedtime > 0x40000000 ) { // time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}
	oldsamplepos = samplepos;

	s_soundtime = buffers * fullsamples + samplepos / dma.channels;

#if 0
// check to make sure that we haven't overshot
	if ( s_paintedtime < s_soundtime ) {
		Com_DPrintf( "S_GetSoundtime : overflow\n" );
		s_paintedtime = s_soundtime;
	}
#endif

	if ( dma.submission_chunk < 256 ) {
		s_paintedtime = s_soundtime + s_mixPreStep->value * dma.speed;
	} else {
		s_paintedtime = s_soundtime + dma.submission_chunk;
	}
}

/*
============
S_Update_Mix
============
*/
void S_Update_Mix( void ) {
	unsigned endtime;
	int samps, i;
	static float lastTime = 0.0f;
	float ma, op;
	float thisTime, sane;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}


	for ( i = 0; i < tart; i++ ) {
		if ( pushPop[i].fixedOrigin ) {
			S_ThreadStartSoundEx( pushPop[i].origin, pushPop[i].entityNum, pushPop[i].entityChannel, pushPop[i].sfx, pushPop[i].flags );
		} else {
			S_ThreadStartSoundEx( NULL, pushPop[i].entityNum, pushPop[i].entityChannel, pushPop[i].sfx, pushPop[i].flags );
		}
	}

	tart = 0;

	s_soundPainted = qtrue;

	thisTime = Sys_Milliseconds();

	// Updates s_soundtime
	S_GetSoundtime();

	// clear any sound effects that end before the current time,
	// and start any new sounds
	S_ScanChannelStarts();

	sane = thisTime - lastTime;
	if ( sane < 11 ) {
		sane = 11;          // 85hz
	}

	ma = s_mixahead->value * dma.speed;
	op = s_mixPreStep->value + sane * dma.speed * 0.01;

	if ( op < ma ) {
		ma = op;
	}

	// mix ahead of current position
	endtime = s_soundtime + ma;

	// mix to an even submission block size
	endtime = ( endtime + dma.submission_chunk - 1 )
			  & ~( dma.submission_chunk - 1 );

	// never mix more than the complete buffer
	samps = dma.samples >> ( dma.channels - 1 );
	if ( endtime - s_soundtime > samps ) {
		endtime = s_soundtime + samps;
	}

	SNDDMA_BeginPainting();
	S_PaintChannels( endtime );
	SNDDMA_Submit();

	lastTime = thisTime;
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play_f( void ) {
	int i;
	sfxHandle_t h;
	char name[256];

	i = 1;
	while ( i < Cmd_Argc() ) {
		if ( !Q_strrchr( Cmd_Argv( i ), '.' ) ) {
			Com_sprintf( name, sizeof( name ), "%s.wav", Cmd_Argv( 1 ) );
		} else {
			Q_strncpyz( name, Cmd_Argv( i ), sizeof( name ) );
		}
		h = S_RegisterSound( name, qfalse );
		if ( h ) {
			S_StartLocalSound( h, CHAN_LOCAL_SOUND );
		}
		i++;
	}
}

void S_Music_f( void ) {
	int c;

	c = Cmd_Argc();

	if ( c == 2 ) {
		S_StartBackgroundTrack( Cmd_Argv( 1 ), Cmd_Argv( 1 ) );
	} else if ( c == 3 ) {
		S_StartBackgroundTrack( Cmd_Argv( 1 ), Cmd_Argv( 2 ) );
		Q_strncpyz( streamingSounds[0].loop, Cmd_Argv( 2 ), sizeof( streamingSounds[0].loop ) );
	} else {
		Com_Printf( "music <musicfile> [loopfile]\n" );
		return;
	}

}

// Ridah, just for testing the streaming sounds
void S_StreamingSound_f( void ) {
	int c;

	c = Cmd_Argc();

	if ( c == 2 ) {
		S_StartStreamingSound( Cmd_Argv( 1 ), 0, -1, 0, 0 );
	} else if ( c == 5 ) {
		S_StartStreamingSound( Cmd_Argv( 1 ), 0, atoi( Cmd_Argv( 2 ) ), atoi( Cmd_Argv( 3 ) ), atoi( Cmd_Argv( 4 ) ) );
	} else {
		Com_Printf( "streamingsound <soundfile> [entnum channel attenuation]\n" );
		return;
	}

}

void S_SoundList_f( void ) {
	int i;
	sfx_t   *sfx;
	int size, total;
	char type[4][16];
	char mem[2][16];

	strcpy( type[0], "16bit" );
	strcpy( type[1], "adpcm" );
	strcpy( type[2], "daub4" );
	strcpy( type[3], "mulaw" );
	strcpy( mem[0], "paged out" );
	strcpy( mem[1], "resident " );
	total = 0;
	for ( sfx = s_knownSfx, i = 0 ; i < s_numSfx ; i++, sfx++ ) {
		size = sfx->soundLength;
		total += size;
		Com_Printf( "%6i[%s] : %s[%s]\n", size, type[sfx->soundCompressionMethod], sfx->soundName, mem[sfx->inMemory] );
	}
	Com_Printf( "Total resident: %i\n", total );
	S_DisplayFreeMemory();
}


/*
===============================================================================

STREAMING SOUND

===============================================================================
*/

int FGetLittleLong( fileHandle_t f ) {
	int v;

	FS_Read( &v, sizeof( v ), f );

	return LittleLong( v );
}

int FGetLittleShort( fileHandle_t f ) {
	short v;

	FS_Read( &v, sizeof( v ), f );

	return LittleShort( v );
}

// returns the length of the data in the chunk, or 0 if not found
int S_FindWavChunk( fileHandle_t f, char *chunk ) {
	char name[5];
	int len;
	int r;

	name[4] = 0;
	len = 0;
	r = FS_Read( name, 4, f );
	if ( r != 4 ) {
		return 0;
	}
	len = FGetLittleLong( f );
	if ( len < 0 || len > 0xfffffff ) {
		len = 0;
		return 0;
	}
	len = ( len + 1 ) & ~1;      // pad to word boundary
//	s_nextWavChunk += len + 8;

	if ( strcmp( name, chunk ) ) {
		return 0;
	}

	return len;
}

/*
======================
S_StartBackgroundTrack
======================
*/
void S_StartBackgroundTrack( const char *intro, const char *loop ) {
	int len;
	char dump[16];
	char name[MAX_QPATH];
	int i;
	streamingSound_t *ss;
	fileHandle_t fh;

//	if (!s_soundStarted  || !crit) {
//		return;
//	}

	Sys_EnterCriticalSection( crit );

	if ( !intro ) {
		intro = "";
	}
	if ( !loop || !loop[0] ) {
		loop = intro;
	}
	Com_DPrintf( "S_StartBackgroundTrack( %s, %s )...\n", intro, loop );

	// music is always track 0
	i = 0;

	ss = &streamingSounds[i];

	Q_strncpyz( ss->loop, loop, sizeof( ss->loop ) - 4 );

	Q_strncpyz( name, intro, sizeof( name ) - 4 );
	COM_DefaultExtension( name, sizeof( name ), ".wav" );

	// close the current sound if present, but DON'T reset s_rawend
	if ( ss->file ) {
		Sys_EndStreamedFile( ss->file );
		FS_FCloseFile( ss->file );
		ss->file = 0;
	}

	if ( !intro[0] ) {
		Com_DPrintf( "Fail to start: %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	ss->channel = 0;
	ss->entnum = -1;
	ss->attenuation = 0;

	//
	// open up a wav file and get all the info
	//
	FS_FOpenFileRead( name, &fh, qtrue );
	if ( !fh ) {
		Com_Printf( "Couldn't open streaming sound file %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	// skip the riff wav header

	FS_Read( dump, 12, fh );

	if ( !S_FindWavChunk( fh, "fmt " ) ) {
		Com_Printf( "No fmt chunk in %s\n", name );
		FS_FCloseFile( fh );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	// save name for soundinfo
	ss->info.format = FGetLittleShort( fh );
	ss->info.channels = FGetLittleShort( fh );
	ss->info.rate = FGetLittleLong( fh );
	FGetLittleLong( fh );
	FGetLittleShort( fh );
	ss->info.width = FGetLittleShort( fh ) / 8;

	if ( ss->info.format != WAV_FORMAT_PCM ) {
		FS_FCloseFile( fh );
		Com_Printf( "Not a microsoft PCM format wav: %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	if ( ss->info.channels != 2 || ss->info.rate != 22050 ) {
		Com_Printf( "WARNING: music file %s is not 22k stereo\n", name );
	}

	if ( ( len = S_FindWavChunk( fh, "data" ) ) == 0 ) {
		FS_FCloseFile( fh );
		Com_Printf( "No data chunk in %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	ss->info.samples = len / ( ss->info.width * ss->info.channels );

	ss->samples = ss->info.samples;

	//
	// start the background streaming
	//
	Sys_BeginStreamedFile( fh, 0x10000 );

	ss->file = fh;
	ss->kill = qfalse;
	numStreamingSounds++;

	Com_DPrintf( "S_StartBackgroundTrack - Success\n", intro, loop );
	Sys_LeaveCriticalSection( crit );
}

/*
======================
S_StartStreamingSound

  FIXME: record the starting cg.time of the sound, so we can determine the
  position by looking at the current cg.time, this way pausing or loading a
  savegame won't screw up the timing of important sounds
======================
*/
void S_StartStreamingSound( const char *intro, const char *loop, int entnum, int channel, int attenuation ) {
	int len;
	char dump[16];
	char name[MAX_QPATH];
	int i;
	streamingSound_t *ss;
	fileHandle_t fh;

	if ( !crit || !s_soundStarted || s_soundMuted || cls.state != CA_ACTIVE ) {
		return;
	}

	Sys_EnterCriticalSection( crit );
	if ( !intro || !intro[0] ) {
		if ( loop && loop[0] ) {
			intro = loop;
		} else {
			intro = "";
		}
	}
	Com_DPrintf( "S_StartStreamingSound( %s, %s, %i, %i, %i )\n", intro, loop, entnum, channel, attenuation );

	// look for a free track, but first check for overriding a currently playing sound for this entity
	ss = NULL;
	if ( entnum >= 0 ) {
		for ( i = 1; i < MAX_STREAMING_SOUNDS; i++ ) {    // track 0 is music/cinematics
			if ( !streamingSounds[i].file ) {
				continue;
			}
			// check to see if this character currently has another sound streaming on the same channel
			if ( ( channel != CHAN_AUTO ) && ( streamingSounds[i].entnum >= 0 ) && ( streamingSounds[i].channel == channel ) && ( streamingSounds[i].entnum == entnum ) ) {
				// found a match, override this channel
				streamingSounds[i].kill = qtrue;
				ss = &streamingSounds[i];   // use this track to start the new stream
				break;
			}
		}
	}
	if ( !ss ) {
		// no need to override a current stream, so look for a free track
		for ( i = 1; i < MAX_STREAMING_SOUNDS; i++ ) {    // track 0 is music/cinematics
			if ( !streamingSounds[i].file ) {
				ss = &streamingSounds[i];
				break;
			}
		}
	}
	if ( !ss ) {
		if ( !s_mute->integer ) {  // don't do the print if you're muted
			Com_Printf( "S_StartStreamingSound: No free streaming tracks\n" );
		}
		Sys_LeaveCriticalSection( crit );
		return;
	}

	if ( loop ) {
		Q_strncpyz( ss->loop, loop, sizeof( ss->loop ) - 4 );
	} else {
		ss->loop[0] = 0;
	}

	Q_strncpyz( name, intro, sizeof( name ) - 4 );
	COM_DefaultExtension( name, sizeof( name ), ".wav" );

	// close the current sound if present, but DON'T reset s_rawend
	if ( ss->file ) {
		Sys_EndStreamedFile( ss->file );
		FS_FCloseFile( ss->file );
		ss->file = 0;
	}

	if ( !intro[0] ) {
		Sys_LeaveCriticalSection( crit );
		return;
	}

	//
	// open up a wav file and get all the info
	//
	FS_FOpenFileRead( name, &fh, qtrue );
	if ( !fh ) {
		Com_Printf( "Couldn't open streaming sound file %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	// skip the riff wav header

	FS_Read( dump, 12, fh );

	if ( !S_FindWavChunk( fh, "fmt " ) ) {
		Com_Printf( "No fmt chunk in %s\n", name );
		FS_FCloseFile( fh );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	// save name for soundinfo
	ss->info.format = FGetLittleShort( fh );
	ss->info.channels = FGetLittleShort( fh );
	ss->info.rate = FGetLittleLong( fh );
	FGetLittleLong(  fh );
	FGetLittleShort(  fh );
	ss->info.width = FGetLittleShort( fh ) / 8;

	if ( ss->info.format != WAV_FORMAT_PCM ) {
		FS_FCloseFile( fh );
		Com_Printf( "Not a microsoft PCM format wav: %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	//if ( ss->info.channels != 2 || ss->info.rate != 22050 ) {
	//	Com_Printf("WARNING: music file %s is not 22k stereo\n", name );
	//}

	if ( ( len = S_FindWavChunk( fh, "data" ) ) == 0 ) {
		FS_FCloseFile( fh );
		Com_Printf( "No data chunk in %s\n", name );
		Sys_LeaveCriticalSection( crit );
		return;
	}

	ss->info.samples = len / ( ss->info.width * ss->info.channels );

	ss->samples = ss->info.samples;
	ss->channel = channel;
	ss->attenuation = attenuation;
	ss->entnum = entnum;
	ss->kill = qfalse;

	//
	// start the background streaming
	//
	Sys_BeginStreamedFile( fh, 0x10000 );

	ss->file = fh;
	numStreamingSounds++;
	Sys_LeaveCriticalSection( crit );
}

/*
======================
S_StopStreamingSound
======================
*/
void S_StopStreamingSound( int index ) {
	if ( !streamingSounds[index].file ) {
		return;
	}
	Sys_EnterCriticalSection( crit );
	streamingSounds[index].kill = qtrue;
	Sys_LeaveCriticalSection( crit );
}

/*
======================
S_StopBackgroundTrack
======================
*/
void S_StopBackgroundTrack( void ) {
	S_StopStreamingSound( 0 );
}

/*
======================
S_UpdateStreamingSounds
======================
*/
void S_UpdateStreamingSounds( void ) {
	int bufferSamples;
	int fileSamples;
	byte raw[30000];        // just enough to fit in a mac stack frame
	int fileBytes;
	int r, i;
	streamingSound_t *ss;
	int     *re, *rp;
	qboolean looped;
	float lvol, rvol;
	int soundMixAheadTime;

//	if (!s_soundStarted  || !crit) {
//		return;
//	}

	if ( s_mute->value ) {  //----(SA)	sound is muted, skip everything
		return;
	}

	soundMixAheadTime = s_soundtime; // + (int)(0.35 * dma.speed);	// allow for talking animations

	//----(SA)	it seems this could potentially be in the wrong place.
	//			The intended purpose is to just quiet all sounds if s_mute is set (like a TV mute button)
	//			however, it seems the location here could potentially cause some streaming sound updates
	//			to not happen properly, so if you mute and un-mute while listening to a conversation
	//			you could screw up the timing.  Is that the case?
	//			Ryan, could you give it a quick once-over to see if this is okay?
	//
	//	(go ahead and delete commentary when you read)


	s_soundPainted = qtrue;

	for ( i = 0, ss = streamingSounds, re = s_rawend, rp = s_rawpainted; i < MAX_STREAMING_SOUNDS; i++, ss++, re++, rp++ ) {
		if ( ss->kill ) {
			fileHandle_t file;
			file = ss->file;
			ss->file = 0;
			Sys_EndStreamedFile( file );
			FS_FCloseFile( file );
			numStreamingSounds--;
			ss->kill = qfalse;
			continue;
		}

		*rp = qfalse;

		if ( !ss->file ) {
			continue;
		}

		// don't bother playing anything if musicvolume is 0
		if ( i == 0 && s_musicVolume->value <= 0 ) {
			continue;
		}
		if ( i > 0 && s_volume->value <= 0 ) {
			continue;
		}

		// see how many samples should be copied into the raw buffer
		if ( *re < soundMixAheadTime ) {    // RF, read a bit ahead of time to allow for talking animations
			*re = soundMixAheadTime;
		}

		looped = qfalse;

		while ( *re < soundMixAheadTime + MAX_RAW_SAMPLES ) {
			bufferSamples = MAX_RAW_SAMPLES - ( *re - soundMixAheadTime );

			// decide how much data needs to be read from the file
			fileSamples = bufferSamples * ss->info.rate / dma.speed;

			// if there are no samples due to be read this frame, abort painting
			// but keep the streaming going, since it might just need to wait until
			// the next frame before it needs to paint some more
			if ( !fileSamples ) {
				break;
			}

			// don't try and read past the end of the file
			if ( fileSamples > ss->samples ) {
				fileSamples = ss->samples;
			}

			// our max buffer size
			fileBytes = fileSamples * ( ss->info.width * ss->info.channels );
			if ( fileBytes > sizeof( raw ) ) {
				fileBytes = sizeof( raw );
				fileSamples = fileBytes / ( ss->info.width * ss->info.channels );
			}

			r = Sys_StreamedRead( raw, 1, fileBytes, ss->file );
			if ( r != fileBytes ) {
				Com_DPrintf( "StreamedRead failure on stream sound\n" );
				ss->kill = qtrue;
				break;
			}

			// byte swap if needed
			S_ByteSwapRawSamples( fileSamples, ss->info.width, ss->info.channels, (short*)raw );

			// calculate the volume
			if ( i == 0 ) {   // music
				lvol = rvol = s_musicVolume->value;
			} else {        // attenuate if required
				if ( ss->entnum >= 0 && ss->attenuation ) {
					int r, l;
					S_SpatializeOrigin( entityPositions[ ss->entnum ], s_volume->value * 255.0f, &l, &r, SOUND_RANGE_DEFAULT );
					if ( ( lvol = ( (float)l / 255.0 ) ) > 1.0 ) {
						lvol = 1.0;
					}
					if ( ( rvol = ( (float)r / 255.0 ) ) > 1.0 ) {
						rvol = 1.0;
					}
				} else {
					lvol = rvol = s_volume->value;
				}
			}

			// add to raw buffer
			S_RawSamples( fileSamples, ss->info.rate,
						  ss->info.width, ss->info.channels, raw, lvol, rvol, i );

			*rp = qtrue;

			ss->samples -= fileSamples;

			if ( !ss->samples ) {

				if ( ss->loop[0] ) {
					// loop
					if ( looped ) {
						// already looped once
						//*re = 0;
						break;
					} else {
						char dump[16];
						Sys_StreamSeek( ss->file, 0, FS_SEEK_SET );
						FS_Read( dump, 12, ss->file );

						if ( !S_FindWavChunk( ss->file, "fmt " ) ) {
							ss->kill = qtrue;
							break;
						}

						// save name for soundinfo
						ss->info.format = FGetLittleShort( ss->file );
						ss->info.channels = FGetLittleShort( ss->file );
						ss->info.rate = FGetLittleLong( ss->file );
						FGetLittleLong( ss->file );
						FGetLittleShort( ss->file );
						ss->info.width = FGetLittleShort( ss->file ) / 8;
						looped = qtrue;
						ss->samples = ss->info.samples;
						if ( ( S_FindWavChunk( ss->file, "data" ) ) == 0 ) {
							ss->kill = qtrue;
							return;
						}
					}
				} else {
					// no loop, just stop
					ss->kill = qtrue;;
					break;
				}

			}
		}
	}
}
