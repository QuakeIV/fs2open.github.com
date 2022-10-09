/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/



#include "globalincs/pstypes.h"

#if !(defined(__APPLE__) || defined(_WIN32))
	#include <AL/al.h>
	#include <AL/alc.h>
#else
	#include "al.h"
	#include "alc.h"
#endif // !__APPLE__ && !_WIN32

#include "sound/ds3d.h"
#include "sound/ds.h"
#include "sound/sound.h"
#include "object/object.h"


int DS3D_inited = FALSE;


// ---------------------------------------------------------------------------------------
// ds3d_update_buffer()
//
//	parameters:		channel	=> identifies the 3D sound to update
//						min		=>	the distance at which sound doesn't get any louder
//						max		=>	the distance at which sound doesn't attenuate any further
//						pos		=> world position of sound
//						vel		=> velocity of the objects producing the sound
//
//	returns:		0		=>		success
//					-1		=>		failure
//
//
int ds3d_update_buffer(int channel, float min, float max, vec3d *pos, vec3d *vel)
{
	if (DS3D_inited == FALSE)
		return 0;

	if ( channel == -1 )
		return 0;

	// as used by DS3D
//	OpenAL_ErrorPrint( alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED) );

	// set the min distance
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_REFERENCE_DISTANCE, min) );

	// set the max distance
//	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_MAX_DISTANCE, max) );
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_MAX_DISTANCE, 40000.0f) );
	
	// set rolloff factor
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_ROLLOFF_FACTOR, 1.0f) );
		
	// set doppler
	OpenAL_ErrorPrint( alDopplerVelocity(10000.0f) );
	OpenAL_ErrorPrint( alDopplerFactor(0.0f) );  // TODO: figure out why using a value of 1 sounds bad

	// set the buffer position
	if ( pos != NULL ) {
		ALfloat alpos[] = { pos->xyz.x, pos->xyz.y, pos->xyz.z };
		OpenAL_ErrorPrint( alSourcefv(Channels[channel].source_id, AL_POSITION, alpos) );
	}

	// set the buffer velocity
	if ( vel != NULL ) {
		ALfloat alvel[] = { vel->xyz.x, vel->xyz.y, vel->xyz.z };
		OpenAL_ErrorPrint( alSourcefv(Channels[channel].source_id, AL_VELOCITY, alvel) );
	} else {
		ALfloat alvel[] = { 0.0f, 0.0f, 0.0f };
		OpenAL_ErrorPrint( alSourcefv(Channels[channel].source_id, AL_VELOCITY, alvel) );
	}

	return 0;
}


// ---------------------------------------------------------------------------------------
// ds3d_update_listener()
//
//	returns:		0		=>		success
//					-1		=>		failure
//
int ds3d_update_listener(vec3d *pos, vec3d *vel, matrix *orient)
{
	if (DS3D_inited == FALSE)
		return 0;

	// set the listener position
	if ( pos != NULL ) {
		OpenAL_ErrorPrint( alListener3f(AL_POSITION, pos->xyz.x, pos->xyz.y, pos->xyz.z) );
	}

	// set the listener velocity
	if ( vel != NULL ) {
		OpenAL_ErrorPrint( alListener3f(AL_VELOCITY, vel->xyz.x, vel->xyz.y, vel->xyz.z) );
	}

	// set the listener orientation
	if ( orient != NULL ) {
		// uvec is up/top vector, fvec is at/front vector
		ALfloat list_orien[] = { orient->vec.fvec.xyz.x, orient->vec.fvec.xyz.y, orient->vec.fvec.xyz.z,
									orient->vec.uvec.xyz.x, orient->vec.uvec.xyz.y, orient->vec.uvec.xyz.z };
		OpenAL_ErrorPrint( alListenerfv(AL_ORIENTATION, list_orien) );
	}

	return 0;
}


// ---------------------------------------------------------------------------------------
// ds3d_init()
//
// Initialize the DirectSound3D system.  Call the initialization for the pDS3D_listener
// 
// returns:     -1	=> init failed
//              0		=> success
int ds3d_init(int voice_manager_required)
{
	if ( DS3D_inited == TRUE )
		return 0;
	
	DS3D_inited = TRUE;
	return 0;
}


// ---------------------------------------------------------------------------------------
// ds3d_close()
//
// De-initialize the DirectSound3D system
// 
void ds3d_close()
{
	if ( DS3D_inited == FALSE )
		return;

	DS3D_inited = FALSE;
}
