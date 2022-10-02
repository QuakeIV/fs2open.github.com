/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/



#include "globalincs/pstypes.h"
#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
//#include <initguid.h>
#include "mm/mmreg.h"
#define STUB_FUNCTION
#endif
#include "cfile/cfile.h"
#include "sound/ds.h"
#include "sound/ds3d.h"
#include "osapi/osapi.h"
#include "sound/dscap.h"

#if !(defined(__APPLE__) || defined(_WIN32))
	#include <AL/al.h>
	#include <AL/alc.h>
#else
	#include "al.h"
	#include "alc.h"
#endif // !__APPLE__ && !_WIN32

extern unsigned short UserSampleRate, UserSampleBits; //in sound.h

typedef struct sound_buffer
{
	ALuint buf_id;		// OpenAL buffer id
	int source_id;		// source index this buffer is currently bound to

	int frequency;
	int bits_per_sample;
	int nchannels;
	int nseconds;
	int nbytes;

	sound_buffer(): buf_id(0), source_id(-1), frequency(0), bits_per_sample(0), nchannels(0), nseconds(0), nbytes(0) {}
} sound_buffer;


static int MAX_CHANNELS = 32;		// initialized properly in ds_init_channels()
channel *Channels = NULL;
static int channel_next_sig = 1;

const int BUFFER_BUMP = 50;
SCP_vector<sound_buffer> sound_buffers;

extern int Snd_sram;					// mem (in bytes) used up by storing sounds in system memory

static int Ds_use_ds3d = 0;
static int Ds_use_a3d = 0;
static int Ds_use_eax = 0;

static int AL_play_position = 0;

#ifndef AL_BYTE_LOKI
// in case it's not defined by older/other drivers
#define AL_BYTE_LOKI	0x100C
#endif

ALCdevice *ds_sound_device = NULL;
ALCcontext *ds_sound_context = NULL;

ALCint AL_minor_version = 0;

//--------------------------------------------------------------------------
// openal_error_string()
//
// Returns the human readable error string if there is an error or NULL if not
//
const char* openal_error_string(int get_alc)
{
	int i;

	if (get_alc) {
		// Apple implementation requires a valid device to give a valid error msg
		i = alcGetError(ds_sound_device);

		if ( i != ALC_NO_ERROR )
			return (const char*) alcGetString(NULL, i);
	}
	else {
		i = alGetError();

		if ( i != AL_NO_ERROR )
			return (const char*)alGetString(i);
	}

	return NULL;
}

int ds_vol_lookup[101];						// lookup table for direct sound volumes
int ds_initialized = FALSE;

//--------------------------------------------------------------------------
// ds_is_3d_buffer()
//
// Determine if a secondary buffer is a 3d secondary buffer.
//
int ds_is_3d_buffer(int sid)
{
	// they are all 3d
	if ( sid >= 0 ) {
		return 1;
	}
	return 0;
}

//--------------------------------------------------------------------------
//  ds_build_vol_lookup()
//
//  Fills up the ds_vol_lookup[] tables that converts from a volume in the form
//  0.0 -> 1.0 to -10000 -> 0 (this is the DirectSound method, where units are
//  hundredths of decibls)
//
void ds_build_vol_lookup()
{
	int	i;
	float	vol;

	ds_vol_lookup[0] = -10000;
	for ( i = 1; i <= 100; i++ ) {
		vol = i / 100.0f;
		ds_vol_lookup[i] = fl2i( (log(vol) / log(2.0f)) * 1000.0f);
	}
}


//--------------------------------------------------------------------------
// ds_convert_volume()
//
// Takes volume between 0.0f and 1.0f and converts into
// DirectSound style volumes between -10000 and 0.
int ds_convert_volume(float volume)
{
	int index;

	index = fl2i(volume * 100.0f);
	if ( index > 100 )
		index = 100;
	if ( index < 0 )
		index = 0;

	return ds_vol_lookup[index];
}

//--------------------------------------------------------------------------
// ds_get_percentage_vol()
//
// Converts -10000 -> 0 range volume to 0 -> 1
float ds_get_percentage_vol(int ds_vol)
{
	double vol;
	vol = pow(2.0, ds_vol/1000.0);
	return (float)vol;
}

// ---------------------------------------------------------------------------------------
// ds_parse_sound() 
//
// Parse a wave file.
//
// parameters:		filename			=> file of sound to parse
//						dest				=> address of pointer of where to store raw sound data (output parm)
//						dest_size		=> number of bytes of sound data stored (output parm)
//						header			=> address of pointer to a WAVEFORMATEX struct (output parm)
//						ovf				=> pointer to a OggVorbis_File struct, OGG vorbis only (output parm)
//
// returns:			0					=> wave file successfully parsed
//						-1					=> error
//
//	NOTE: memory is malloced for the header and dest (if not OGG) in this function.  It is the responsibility
//			of the caller to free this memory later.
//
int ds_parse_sound(CFILE* fp, ubyte **dest, uint *dest_size, WAVEFORMATEX **header, bool ogg, OggVorbis_File *ovf)
{
	PCMWAVEFORMAT	PCM_header;
	ushort			cbExtra = 0;
	unsigned int	tag, size, next_chunk;
	bool			got_fmt = false, got_data = false;

	// some preinit stuff, could be done from calling function but this should guarantee it's right
	*dest = NULL;
	*dest_size = 0;

	if (fp == NULL)
		return -1;


	// if we should have a Vorbis file then try for it
	if (ogg) {
		if (ovf == NULL) {
			Int3();
			return -1;
		}

		// Check for OGG Vorbis first
		if ( !ov_open_callbacks(fp, ovf, NULL, 0, cfile_callbacks) ) {
			// got one, now read all of the needed header info
			ov_info(ovf, -1);

			// we only support one logical bitstream
			if ( ov_streams(ovf) != 1 ) {
				nprintf(( "Sound", "SOUND ==> OGG reading error: We don't support bitstream changes!\n" ));
				return -1;
			}

			if ( (*header = (WAVEFORMATEX *) vm_malloc ( sizeof(WAVEFORMATEX) )) != NULL ) {
				(*header)->wFormatTag = OGG_FORMAT_VORBIS;
				(*header)->nChannels = (ushort)ovf->vi->channels;
				(*header)->nSamplesPerSec = ovf->vi->rate;
				(*header)->wBitsPerSample = 16;								//OGGs always decoded at 16 bits here
				(*header)->nBlockAlign = (ushort)(ovf->vi->channels * 2);
				(*header)->nAvgBytesPerSec =  ovf->vi->rate * ovf->vi->channels * 2;

				//WMC - Total samples * channels * bits/sample
				*dest_size = (uint)(ov_pcm_total(ovf, -1) * ovf->vi->channels * 2);
			} else {
				Assert( 0 );
				return -1;
			}

			// we're all good, can leave now
			return 0;
		}
	}
	// otherwise we assime Wave format
	else {
		// Skip the "RIFF" tag and file size (8 bytes)
		// Skip the "WAVE" tag (4 bytes)
		// IMPORTANT!! Look at snd_load before even THINKING about changing this.
		cfseek( fp, 12, CF_SEEK_SET );

		// Now read RIFF tags until the end of file

		while (1) {
			if ( cfread( &tag, sizeof(uint), 1, fp ) != 1 )
				break;

			tag = INTEL_INT( tag );

			if ( cfread( &size, sizeof(uint), 1, fp ) != 1 )
				break;

			size = INTEL_INT( size );

			next_chunk = cftell(fp) + size;

			switch (tag)
			{
				case 0x20746d66:		// The 'fmt ' tag
				{
					//nprintf(("Sound", "SOUND => size of fmt block: %d\n", size));
					PCM_header.wf.wFormatTag		= cfread_ushort(fp);
					PCM_header.wf.nChannels			= cfread_ushort(fp);
					PCM_header.wf.nSamplesPerSec	= cfread_uint(fp);
					PCM_header.wf.nAvgBytesPerSec	= cfread_uint(fp);
					PCM_header.wf.nBlockAlign		= cfread_ushort(fp);
					PCM_header.wBitsPerSample		= cfread_ushort(fp);

					if (PCM_header.wf.wFormatTag != WAVE_FORMAT_PCM)
						cbExtra = cfread_ushort(fp);

					// Allocate memory for WAVEFORMATEX structure + extra bytes
					if ( (*header = (WAVEFORMATEX *) vm_malloc ( sizeof(WAVEFORMATEX)+cbExtra )) != NULL ) {
						// Copy bytes from temporary format structure
						memcpy (*header, &PCM_header, sizeof(PCM_header));
						(*header)->cbSize = cbExtra;

						// Read those extra bytes, append to WAVEFORMATEX structure
						if (cbExtra != 0)
							cfread( ((ubyte *)(*header) + sizeof(WAVEFORMATEX)), cbExtra, 1, fp);
					} else {
						Assert(0);		// malloc failed
					}

					got_fmt = true;
	
					break;
				}

				case 0x61746164:		// the 'data' tag
				{
					*dest_size = size;

					(*dest) = (ubyte *)vm_malloc(size);
					Assert( *dest != NULL );

					cfread( *dest, size, 1, fp );

					got_data = true;

					break;
				}

				default:	// unknown, skip it
					break;
			}

			// This is here so that we can avoid reading data that we don't understand or properly handle.
			// We could do this just as well by checking the RIFF size, but this is easier - taylor
			if (got_fmt && got_data)
				break;

			cfseek( fp, next_chunk, CF_SEEK_SET );
		}

		// we're all good, can leave now
		return 0;
	}

	return -1;
}

// ---------------------------------------------------------------------------------------
// ds_parse_sound_info() 
//
// Parse a a sound file, any format, and store the info in "s_info".
//
int ds_parse_sound_info(char *real_filename, sound_info *s_info)
{
	PCMWAVEFORMAT	PCM_header;
	uint			tag, size, next_chunk;
	bool			got_fmt = false, got_data = false;
	OggVorbis_File	ovf;
	int				rc, FileSize, FileOffset;
	char			fullpath[MAX_PATH];
	char			filename[MAX_FILENAME_LEN];
	const int		NUM_EXT = 2;
	const char		*audio_ext[NUM_EXT] = { ".ogg", ".wav" };


	if ( (real_filename == NULL) || (s_info == NULL) )
		return -1;


	// remove extension
	strcpy_s( filename, real_filename );
	char *p = strrchr(filename, '.');
	if ( p ) *p = 0;

	rc = cf_find_file_location_ext(filename, NUM_EXT, audio_ext, CF_TYPE_ANY, sizeof(fullpath) - 1, fullpath, &FileSize, &FileOffset);

	if (rc < 0)
		return -1;

	// open the file
	CFILE *fp = cfopen_special(fullpath, "rb", FileSize, FileOffset);

	if (fp == NULL)
		return -1;


	// Ogg Vorbis
	if (rc == 0) {
		if ( !ov_open_callbacks(fp, &ovf, NULL, 0, cfile_callbacks) ) {
			// got one, now read all of the needed header info
			ov_info(&ovf, -1);

			// we only support one logical bitstream
			if ( ov_streams(&ovf) != 1 ) {
				nprintf(( "Sound", "SOUND ==> OGG reading error: We don't support bitstream changes!\n" ));
				return -1;
			}

			s_info->format = OGG_FORMAT_VORBIS;
			s_info->n_channels = (ushort)ovf.vi->channels;
			s_info->sample_rate = ovf.vi->rate;
			s_info->bits = 16;								//OGGs always decoded at 16 bits here
			s_info->n_block_align = (ushort)(ovf.vi->channels * 2);
			s_info->avg_bytes_per_sec = ovf.vi->rate * ovf.vi->channels * 2;

			s_info->size = (uint)(ov_pcm_total(&ovf, -1) * ovf.vi->channels * 2);

			ov_clear(&ovf);
	
			// we're all good, can leave now
			goto Done;
		}
	}
	// PCM Wave
	else if (rc == 1) {
		// Skip the "RIFF" tag and file size (8 bytes)
		// Skip the "WAVE" tag (4 bytes)
		// IMPORTANT!! Look at snd_load before even THINKING about changing this.
		cfseek( fp, 12, CF_SEEK_SET );

		// Now read RIFF tags until the end of file

		while (1) {
			if ( cfread( &tag, sizeof(uint), 1, fp ) != 1 )
				break;

			tag = INTEL_INT( tag );

			if ( cfread( &size, sizeof(uint), 1, fp ) != 1 )
				break;

			size = INTEL_INT( size );

			next_chunk = cftell(fp) + size;

			switch (tag)
			{
				case 0x20746d66:		// The 'fmt ' tag
					PCM_header.wf.wFormatTag		= cfread_ushort(fp);
					PCM_header.wf.nChannels			= cfread_ushort(fp);
					PCM_header.wf.nSamplesPerSec	= cfread_uint(fp);
					PCM_header.wf.nAvgBytesPerSec	= cfread_uint(fp);
					PCM_header.wf.nBlockAlign		= cfread_ushort(fp);
					PCM_header.wBitsPerSample		= cfread_ushort(fp);

					s_info->format = PCM_header.wf.wFormatTag;
					s_info->n_channels = PCM_header.wf.nChannels;
					s_info->sample_rate = PCM_header.wf.nSamplesPerSec;
					s_info->bits = PCM_header.wBitsPerSample;
					s_info->n_block_align = PCM_header.wf.nBlockAlign;
					s_info->avg_bytes_per_sec =  PCM_header.wf.nAvgBytesPerSec;

					got_fmt = true;
	
					break;

				case 0x61746164:		// the 'data' tag
					s_info->size = size;
					got_data = true;

					break;

				default:
					break;
			}

			if (got_fmt && got_data)
				goto Done;

			cfseek( fp, next_chunk, CF_SEEK_SET );
		}
	}

	return -1;

Done:
	cfclose(fp);
	return 0;
}

// ---------------------------------------------------------------------------------------
// ds_get_sid()
//
//	
int ds_get_sid()
{
    sound_buffer new_buffer;
    uint i;

    for (i = 0; i < sound_buffers.size(); i++)
    {
        if (sound_buffers[i].buf_id == 0)
            return (int)i;
    }

    // if we need to, bump the reserve limit (helps prevent memory fragmentation)
    if (sound_buffers.size() == sound_buffers.capacity())
        sound_buffers.reserve(sound_buffers.size() + BUFFER_BUMP);

    sound_buffers.push_back(new_buffer);

    return (int)(sound_buffers.size() - 1);
}

// ---------------------------------------------------------------------------------------
// ds_get_hid()
//
//	
int ds_get_hid()
{
	return -1;
}

// ---------------------------------------------------------------------------------------
// Load a DirectSound secondary buffer with sound data.  The sounds data for
// game sounds are stored in the DirectSound secondary buffers, and are 
// duplicated as needed and placed in the Channels[] array to be played.
// 
//
// parameters:  
//					 sid				  => pointer to software id for sound ( output parm)
//					 hid				  => pointer to hardware id for sound ( output parm)
//					 final_size		  => pointer to storage to receive uncompressed sound size (output parm)
//              header          => pointer to a WAVEFORMATEX structure
//					 si				  => sound_info structure, contains details on the sound format
//					 flags			  => buffer properties ( DS_HARDWARE , DS_3D )
//
// returns:     -1           => sound effect could not loaded into a secondary buffer
//               0           => sound effect successfully loaded into a secondary buffer
//
//
// NOTE: this function is slow, especially when sounds are loaded into hardware.  Don't call this
// function from within gameplay.
//
int ds_load_buffer(int *sid, int *hid, int *final_size, void *header, sound_info *si, int flags)
{
	Assert( final_size != NULL );
	Assert( header != NULL );
	Assert( si != NULL );

	// All sounds are required to have a software buffer

	*sid = ds_get_sid();
	if ( *sid == -1 ) {
		nprintf(("Sound","SOUND ==> No more sound buffers available\n"));
		return -1;
	}

	ALuint pi;
	OpenAL_ErrorCheck( alGenBuffers (1, &pi), return -1 );

	ALenum format;
	ALsizei size;
	ALint bits, bps;
	ALuint frequency;
	ALvoid *data = NULL;
	int byte_order = 0, section, last_section = -1;

	// the below two covnert_ variables are only used when the wav format is not
	// PCM.  DirectSound only takes PCM sound data, so we must convert to PCM if required
	ubyte *convert_buffer = NULL;		// storage for converted wav file
	int convert_len;					// num bytes of converted wav file
	uint src_bytes_used;				// number of source bytes actually converted (should always be equal to original size)
	int rc;
	WAVEFORMATEX *pwfx = (WAVEFORMATEX *)header;


	switch (si->format) {
		case WAVE_FORMAT_PCM:
			Assert( si->data != NULL );
			bits = si->bits;
			bps  = si->avg_bytes_per_sec;
			size = si->size;
			data = si->data;
			break;

		case OGG_FORMAT_VORBIS:
			nprintf(( "Sound", "SOUND ==> converting sound from OGG to PCM\n" ));

			src_bytes_used = 0;
			convert_buffer = (ubyte*)vm_malloc(si->size);
			Assert(convert_buffer != NULL);

			if (convert_buffer == NULL)
				return -1;

			while (src_bytes_used < si->size) {
				rc = ov_read(&si->ogg_info, (char *) convert_buffer + src_bytes_used, si->size - src_bytes_used, byte_order, si->bits / 8, 1, &section);

				// fail if the bitstream changes, shouldn't get this far if that's the case though
				if ((last_section != -1) && (last_section != section)) {
					nprintf(( "Sound", "SOUND ==> OGG reading error: We don't support bitstream changes!\n" ));
					vm_free(convert_buffer);
					convert_buffer = NULL;
					return -1;
				}

				if (rc == OV_EBADLINK) {
					vm_free(convert_buffer);
					convert_buffer = NULL;
					return -1;
				} else if (rc == 0) {
					break;
				} else if (rc > 0) {
					last_section = section;
					src_bytes_used += rc;
				}
			}

			bits = si->bits;
			bps = (((si->n_channels * bits) / 8) * si->sample_rate);
			size = (int)src_bytes_used;
			data = convert_buffer;

			// we're done with ogg stuff so clean it up
			ov_clear(&si->ogg_info);

			nprintf(( "Sound", "SOUND ==> Coverted sound from OGG to PCM successfully\n" ));
			break;

		default:
			STUB_FUNCTION;
			return -1;
	}

	/* format is now in pcm */
	frequency = si->sample_rate;

	if (bits == 16) {
		if (si->n_channels == 2) {
			format = AL_FORMAT_STEREO16;
		} else if (si->n_channels == 1) {
			format = AL_FORMAT_MONO16;
		} else {
			return -1;
		}
	} else if (bits == 8) {
		if (si->n_channels == 2) {
			format = AL_FORMAT_STEREO8;
		} else if (si->n_channels == 1) {
			format = AL_FORMAT_MONO8;
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	Snd_sram += size;
	*final_size = size;

	OpenAL_ErrorCheck( alBufferData(pi, format, data, size, frequency), return -1 );

	sound_buffers[*sid].buf_id = pi;
	sound_buffers[*sid].source_id = -1;
	sound_buffers[*sid].frequency = frequency;
	sound_buffers[*sid].bits_per_sample = bits;
	sound_buffers[*sid].nchannels = si->n_channels;
	sound_buffers[*sid].nseconds = size / bps;
	sound_buffers[*sid].nbytes = size;

	if ( convert_buffer )
		vm_free( convert_buffer );

	return 0;
}

// ---------------------------------------------------------------------------------------
// ds_init_channels()
//
// init the Channels[] array
//
void ds_init_channels()
{
	int i, n;
	ALuint *sids;
	sids = new ALuint[MAX_CHANNELS];

	// -------------------------------------------------------------------------
	// Begin crazy little error check...  This will try and generate up to MAX_CHANNELS worth
	// of sources and if there is an error break off and reset MAX_CHANNELS to that number.  This
	// makes sure that we don't try to make more sources than the implementation can handle.
	//
	// NOTE: This doesn't protect against bad drivers so it's possible to generate more than
	//       we could actually use.  In that case it will likely segfault.

	// clear the current error buffer before doing anything else
	n = alGetError();
	while ( n != AL_NO_ERROR ) {
		n = alGetError();
	}

	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		alGenSources( 1, &sids[i] );
		n = alGetError();
		if ( n != AL_NO_ERROR )
			break;
	}

	// if we didn't make all of them then reset the max and give a Warning message
	if ( i != MAX_CHANNELS ) {
		nprintf(("Warning", "OpenAL: Restricting MAX_CHANNELS to %i (default: %i)\n", i, MAX_CHANNELS));
		MAX_CHANNELS = i;
	}

	// now we have to delete them of course so that the game can make the real ones
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( (sids[i] != 0) && alIsSource(sids[i]) )
			OpenAL_ErrorPrint( alDeleteSources(1, &sids[i]) );
	}

	// cleanup
	delete[] sids;

	// ... End crazy little error check
	// -------------------------------------------------------------------------

	Channels = (channel*) vm_malloc(sizeof(channel) * MAX_CHANNELS);
	if (Channels == NULL) {
		Error(LOCATION, "Unable to allocate %d bytes for %d audio channels.", sizeof(channel) * MAX_CHANNELS, MAX_CHANNELS);
	}

	memset( Channels, 0, sizeof(channel) * MAX_CHANNELS );

	// init the channels
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		Channels[i].source_id = 0;
		Channels[i].buf_id = -1;
		Channels[i].sig = -1;
		Channels[i].snd_id = -1;
	}
}

// ---------------------------------------------------------------------------------------
// ds_init_software_buffers()
//
// init the software buffers
//
void ds_init_software_buffers()
{
	sound_buffers.clear();

	// pre-allocate for at least BUFFER_BUMP buffers
	sound_buffers.reserve( BUFFER_BUMP );
}

// ---------------------------------------------------------------------------------------
// ds_init_hardware_buffers()
//
// init the hardware buffers
//
void ds_init_hardware_buffers()
{
	//	STUB_FUNCTION;	// not needed with openal (CM)
	return;
}

// ---------------------------------------------------------------------------------------
// ds_init_buffers()
//
// init the both the software and hardware buffers
//
void ds_init_buffers()
{
	ds_init_software_buffers();
	ds_init_hardware_buffers();
}

// Load the dsound.dll, and get funtion pointers
// exit:	0	->	dll loaded successfully
//			!0	->	dll could not be loaded
int ds_dll_load()
{
	return 0;
}


// Initialize the property set interface.
//
// returns: 0 if successful, otherwise -1.  If successful, the global pPropertySet will
//          set to a non-NULL value.
//
int ds_init_property_set(DWORD sample_rate, WORD sample_bits)
{
	return 0;
}

// ---------------------------------------------------------------------------------------
// ds_init()
//
// returns:     -1           => init failed
//               0           => init success
int ds_init(int use_a3d, int use_eax, unsigned int sample_rate, unsigned short sample_bits)
{
//	NOTE: A3D and EAX are unused in OpenAL
	int attr[] = { ALC_FREQUENCY, (int)sample_rate, ALC_SYNC, AL_FALSE, 0 };
	ALfloat list_orien[] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };

	Ds_use_a3d = 0;
	Ds_use_eax = 0;
//	Ds_use_ds3d = 1;
	Ds_use_ds3d = 0;

	mprintf(("Initializing OpenAL...\n"));

	// FIXME: see function for problem!
//	openal_get_best_device();

	// version check (for 1.0 or 1.1)
	alcGetIntegerv(NULL, ALC_MINOR_VERSION, sizeof(ALCint), &AL_minor_version);

	// we need to clear out all errors before moving on
	alcGetError(NULL);
	alGetError();

	// load OpenAL
#ifdef _WIN32
	// we require OpenAL 1.1 on Windows, so version check it
	if (!AL_minor_version) {
		MessageBox(NULL, "OpenAL 1.1 or newer is required for proper operation.  Please upgrade your OpenAL drivers, which\nare available at http://www.openal.org/downloads.html, and try running the game again.", NULL, MB_OK);
		return -2;
	}

	// restrict to software rather than hardware (the default) devices here by default since
	// we may have 'too many hardware sources' type problems otherwise - taylor
	char *device_spec = os_config_read_string( NULL, "SoundDeviceOAL", "Generic Software" );
	mprintf(("  Using '%s' as OpenAL sound device...\n", device_spec));

	ds_sound_device = alcOpenDevice( (const ALCchar *) device_spec );
#else
	ds_sound_device = alcOpenDevice( NULL );
#endif

	if ( !ds_sound_device )
		goto AL_InitError;

	// Create Sound Device
	OpenAL_C_ErrorCheck( { ds_sound_context = alcCreateContext( ds_sound_device, attr ); }, goto AL_InitError );

	// set the new context as current
	OpenAL_C_ErrorCheck( alcMakeContextCurrent( ds_sound_context ), goto AL_InitError );

	mprintf(( "  OpenAL Vendor     : %s\n", alGetString( AL_VENDOR ) ));
	mprintf(( "  OpenAL Renderer   : %s\n", alGetString( AL_RENDERER ) ));
	mprintf(( "  OpenAL Version    : %s\n", alGetString( AL_VERSION ) ));
	mprintf(( "\n" ));

	// make sure we can actually use AL_BYTE_LOKI (Mac/Win OpenAL doesn't have it)
	AL_play_position = alIsExtensionPresent( "AL_LOKI_play_position" );

	if (AL_play_position)
		mprintf(( "  Using extension \"AL_LOKI_play_position\".\n" ));

	// not a big deal here, but for consitancy sake
	if (Ds_use_ds3d && ds3d_init(0) != 0)
		Ds_use_ds3d = 0;

	// setup default listener position/orientation
	// this is needed for 2D pan
	OpenAL_ErrorPrint( alListener3f(AL_POSITION, 0.0, 0.0, 0.0) );
	OpenAL_ErrorPrint( alListenerfv(AL_ORIENTATION, list_orien) );

	ds_build_vol_lookup();
	ds_init_channels();
	ds_init_buffers();

	// we need to clear out all errors before moving on
	alcGetError(NULL);
	alGetError();

	mprintf(("... OpenAL successfully initialized!\n"));

	return 0;


AL_InitError:
	alcMakeContextCurrent(NULL);

	if (ds_sound_context != NULL) {
		alcDestroyContext(ds_sound_context);
		ds_sound_context = NULL;
	}

	if (ds_sound_device != NULL) {
		alcCloseDevice(ds_sound_device);
		ds_sound_device = NULL;
	}

	return -1;
}

// ---------------------------------------------------------------------------------------
// get_DSERR_text()
//
// returns the text equivalent for the a DirectSound DSERR_ code
//
char *get_DSERR_text(int DSResult)
{
	STUB_FUNCTION;

	return "unknown";
}


// ---------------------------------------------------------------------------------------
// ds_close_channel()
//
// Free a single channel
//
void ds_close_channel(int i)
{
	if ( (Channels[i].source_id != 0) && alIsSource(Channels[i].source_id) ) {
		OpenAL_ErrorPrint( alSourceStop(Channels[i].source_id) );

		OpenAL_ErrorPrint( alDeleteSources(1, &Channels[i].source_id) );

		Channels[i].source_id = 0;
		Channels[i].buf_id = -1;
		Channels[i].sig = -1;
		Channels[i].snd_id = -1;
	}

	return;
}



// ---------------------------------------------------------------------------------------
// ds_close_all_channels()
//
// Free all the channel buffers
//
void ds_close_all_channels()
{
	int		i;

	for (i = 0; i < MAX_CHANNELS; i++)	{
		ds_close_channel(i);
	}
}

// ---------------------------------------------------------------------------------------
// ds_unload_buffer()
//
//
void ds_unload_buffer(int sid, int hid)
{
	if (sid != -1) {
		ALuint buf_id = sound_buffers[sid].buf_id;
		int channel_idx = sound_buffers[sid].source_id;

		if (channel_idx != -1)
			ds_close_channel(channel_idx);

		if ( (buf_id != 0) && alIsBuffer(buf_id) )
			OpenAL_ErrorPrint( alDeleteBuffers(1, &buf_id) );

		sound_buffers[sid].buf_id = 0;
		sound_buffers[sid].source_id = -1;
	}
}

// ---------------------------------------------------------------------------------------
// ds_close_software_buffers()
//
//
void ds_close_software_buffers()
{
	uint i;

	for (i = 0; i < sound_buffers.size(); i++) {
		ALuint buf_id = sound_buffers[i].buf_id;

		if ( (buf_id != 0) && alIsBuffer(buf_id) ) {
			OpenAL_ErrorPrint( alDeleteBuffers(1, &buf_id) );
		}
	}

	sound_buffers.clear();
}

// ---------------------------------------------------------------------------------------
// ds_close_hardware_buffers()
//
//
void ds_close_hardware_buffers()
{
}

// ---------------------------------------------------------------------------------------
// ds_close_buffers()
//
// Free the channel buffers
//
void ds_close_buffers()
{
	ds_close_software_buffers();
	ds_close_hardware_buffers();
}

// ---------------------------------------------------------------------------------------
// ds_close()
//
// Close the DirectSound system
//
void ds_close()
{
	ds_close_buffers();
	ds_close_all_channels();

	// free the Channels[] array, since it was dynamically allocated
	vm_free(Channels);
	Channels = NULL;

	alcMakeContextCurrent(NULL);	// hangs on me for some reason

	if (ds_sound_context != NULL)
		alcDestroyContext(ds_sound_context);

	if (ds_sound_device != NULL)
		alcCloseDevice(ds_sound_device);
}


// ---------------------------------------------------------------------------------------
// ds_get_free_channel()
// 
// Find a free channel to play a sound on.  If no free channels exists, free up one based
// on volume levels.
//
//	input:		new_volume	=>		volume in DS units for sound to play at
//					snd_id		=>		which kind of sound to play
//					priority		=>		DS_MUST_PLAY
//											DS_LIMIT_ONE
//											DS_LIMIT_TWO
//											DS_LIMIT_THREE
//
//	returns:		channel number to play sound on
//					-1 if no channel could be found
//
// NOTE:	snd_id is needed since we limit the number of concurrent samples
//
//
#define DS_MAX_SOUND_INSTANCES 2

int ds_get_free_channel(int new_volume, int snd_id, int priority)
{
	int				i, first_free_channel, limit;
	int				lowest_vol = 0, lowest_vol_index = -1;
	int				instance_count;	// number of instances of sound already playing
	int				lowest_instance_vol, lowest_instance_vol_index;
	channel			*chp;
	int status;

	instance_count = 0;
	lowest_instance_vol = 99;
	lowest_instance_vol_index = -1;
	first_free_channel = -1;

	// Look for a channel to use to play this sample
	for ( i = 0; i < MAX_CHANNELS; i++ )	{
		chp = &Channels[i];

		if ( chp->source_id == 0 ) {
			if ( first_free_channel == -1 )
				first_free_channel = i;

			continue;
		}

		OpenAL_ErrorCheck( alGetSourcei(chp->source_id, AL_SOURCE_STATE, &status), continue );

		if ( status != AL_PLAYING ) {
			if ( first_free_channel == -1 )
				first_free_channel = i;

			ds_close_channel(i);

			continue;
		}
		else {
			if ( chp->snd_id == snd_id ) {
				instance_count++;
				if ( chp->vol < lowest_instance_vol && chp->looping == FALSE ) {
					lowest_instance_vol = chp->vol;
					lowest_instance_vol_index = i;
				}
			}

			if ( chp->vol < lowest_vol && chp->looping == FALSE ) {
				lowest_vol_index = i;
				lowest_vol = chp->vol;
			}
		}
	}

	// determine the limit of concurrent instances of this sound
	switch(priority) {
		case DS_MUST_PLAY:
			limit = 100;
			break;
		case DS_LIMIT_ONE:
			limit = 1;
			break;
		case DS_LIMIT_TWO:
			limit = 2;
			break;
		case DS_LIMIT_THREE:
			limit = 3;
			break;
		default:
			Int3();			// get Alan
			limit = 100;
			break;
	}


	// If we've exceeded the limit, then maybe stop the duplicate if it is lower volume
	if ( instance_count >= limit ) {
		// If there is a lower volume duplicate, stop it.... otherwise, don't play the sound
		if ( lowest_instance_vol_index >= 0 && (Channels[lowest_instance_vol_index].vol <= new_volume) ) {
			ds_close_channel(lowest_instance_vol_index);
			first_free_channel = lowest_instance_vol_index;
		} else {
			first_free_channel = -1;
		}
	} else {
		// there is no limit barrier to play the sound, so see if we've ran out of channels
		if ( first_free_channel == -1 ) {
			// stop the lowest volume instance to play our sound if priority demands it
			if ( lowest_vol_index != -1 && priority == DS_MUST_PLAY ) {
				// Check if the lowest volume playing is less than the volume of the requested sound.
				// If so, then we are going to trash the lowest volume sound.
				if ( Channels[lowest_vol_index].vol <= new_volume ) {
					ds_close_channel(lowest_vol_index);
					first_free_channel = lowest_vol_index;
				}
			}
		}
	}

	if ( (first_free_channel >= 0) && (Channels[first_free_channel].source_id == 0) )
		OpenAL_ErrorCheck( alGenSources(1, &Channels[first_free_channel].source_id), return -1 );

	return first_free_channel;
}

// Create a direct sound buffer in software, without locking any data in
int ds_create_buffer(int frequency, int bits_per_sample, int nchannels, int nseconds)
{
	ALuint i;
	int sid;

	if (!ds_initialized) {
		return -1;
	}

	sid = ds_get_sid();
	if ( sid == -1 ) {
		nprintf(("Sound","SOUND ==> No more OpenAL buffers available\n"));
		return -1;
	}

	OpenAL_ErrorCheck( alGenBuffers(1, &i), return -1 );
	
	sound_buffers[sid].buf_id = i;
	sound_buffers[sid].source_id = -1;
	sound_buffers[sid].frequency = frequency;
	sound_buffers[sid].bits_per_sample = bits_per_sample;
	sound_buffers[sid].nchannels = nchannels;
	sound_buffers[sid].nseconds = nseconds;
	sound_buffers[sid].nbytes = nseconds * (bits_per_sample / 8) * nchannels * frequency;

	return sid;
}

// Lock data into an existing buffer
int ds_lock_data(int sid, unsigned char *data, int size)
{
	STUB_FUNCTION;
/*
	Assert(sid >= 0);

	ALuint buf_id = sound_buffers[sid].buf_id;
	ALenum format;

	if (sound_buffers[sid].bits_per_sample == 16) {
		if (sound_buffers[sid].nchannels == 2) {
			format = AL_FORMAT_STEREO16;
		} else if (sound_buffers[sid].nchannels == 1) {
			format = AL_FORMAT_MONO16;
		} else {
			return -1;
		}
	} else if (sound_buffers[sid].bits_per_sample == 8) {
		if (sound_buffers[sid].nchannels == 2) {
			format = AL_FORMAT_STEREO8;
		} else if (sound_buffers[sid].nchannels == 1) {
			format = AL_FORMAT_MONO8;
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	sound_buffers[sid].nbytes = size;

	OpenAL_ErrorCheck( alBufferData(buf_id, format, data, size, sound_buffers[sid].frequency), return -1 );
*/
	return 0;
}

// Stop a buffer from playing directly
void ds_stop_easy(int sid)
{
	Assert(sid >= 0);

	int cid = sound_buffers[sid].source_id;

	if (cid != -1) {
		ALuint source_id = Channels[cid].source_id;

		OpenAL_ErrorPrint( alSourceStop(source_id) );
	}
}

//	Play a sound without the usual baggage (used for playing back real-time voice)
//
// parameters:  
//					sid			=> software id of sound
//					volume      => volume of sound effect in DirectSound units
int ds_play_easy(int sid, int volume)
{
	if (!ds_initialized)
		return -1;

	int ch_idx = ds_get_free_channel(volume, -1, DS_MUST_PLAY);

	if (ch_idx < 0)
		return -1;

	ALuint source_id = Channels[ch_idx].source_id;

	OpenAL_ErrorPrint( alSourceStop(source_id) );

	if (Channels[ch_idx].buf_id != sid) {
		ALuint buffer_id = sound_buffers[sid].buf_id;

		OpenAL_ErrorCheck( alSourcei(source_id, AL_BUFFER, buffer_id), return -1 );
	}

	Channels[ch_idx].buf_id = sid;

	ALfloat alvol = (volume != -10000) ? powf(10.0f, (float)volume / (-600.0f / log10f(.5f))): 0.0f;

	OpenAL_ErrorPrint( alSourcef(source_id, AL_GAIN, alvol) );

	OpenAL_ErrorPrint( alSourcei(source_id, AL_LOOPING, AL_FALSE) );

	OpenAL_ErrorPrint( alSourcePlay(source_id) );

	return 0;
}

//extern void HUD_add_to_scrollback(char *text, int source);
//extern void HUD_printf(char *format, ...);

// ---------------------------------------------------------------------------------------
// Play a DirectSound secondary buffer.  
// 
//
// parameters:  
//					sid			=> software id of sound
//					hid			=> hardware id of sound ( -1 if not in hardware )
//					snd_id		=>	what kind of sound this is
//					priority		=>		DS_MUST_PLAY
//											DS_LIMIT_ONE
//											DS_LIMIT_TWO
//											DS_LIMIT_THREE
//					volume      => volume of sound effect in DirectSound units
//					pan         => pan of sound in DirectSound units
//             looping     => whether the sound effect is looping or not
//
// returns:    -1          => sound effect could not be started
//              >=0        => sig for sound effect successfully started
//
int ds_play(int sid, int hid, int snd_id, int priority, int volume, int pan, int looping, bool is_voice_msg)
{
	int ch_idx;

	if (!ds_initialized)
		return -1;

	ch_idx = ds_get_free_channel(volume, snd_id, priority);

	if (ch_idx < 0) {
//		nprintf(( "Sound", "SOUND ==> Not playing sound requested at volume %.2f\n", ds_get_percentage_vol(volume) ));
		return -1;
	}

	if (Channels[ch_idx].source_id == 0)
		return -1;

	if ( ds_using_ds3d() ) { }

	// set new position for pan or zero out if none
	ALfloat alpan = (float)pan / MAX_PAN;

	if ( alpan ) {
		OpenAL_ErrorPrint( alSource3f(Channels[ch_idx].source_id, AL_POSITION, alpan, 0.0, 1.0) );
	} else {
		OpenAL_ErrorPrint( alSource3f(Channels[ch_idx].source_id, AL_POSITION, 0.0, 0.0, 0.0) );
	}

	OpenAL_ErrorPrint( alSource3f(Channels[ch_idx].source_id, AL_VELOCITY, 0.0, 0.0, 0.0) );

	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_PITCH, 1.0) );

	ALfloat alvol = (volume != -10000) ? powf(10.0f, (float)volume / (-600.0f / log10f(.5f))): 0.0f;
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_GAIN, alvol) );

	ALint status;
	OpenAL_ErrorCheck( alGetSourcei(Channels[ch_idx].source_id, AL_SOURCE_STATE, &status), return -1 );
		
	if (status == AL_PLAYING)
		OpenAL_ErrorPrint( alSourceStop(Channels[ch_idx].source_id) );


	OpenAL_ErrorCheck( alSourcei(Channels[ch_idx].source_id, AL_BUFFER, sound_buffers[sid].buf_id), return -1 );

	// setup default listener position/orientation
	// this is needed for 2D pan
	OpenAL_ErrorPrint( alListener3f(AL_POSITION, 0.0, 0.0, 0.0) );

	ALfloat list_orien[] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
	OpenAL_ErrorPrint( alListenerfv(AL_ORIENTATION, list_orien) );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_SOURCE_RELATIVE, AL_FALSE) );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_LOOPING, (looping) ? AL_TRUE : AL_FALSE) );

	OpenAL_ErrorPrint( alSourcePlay(Channels[ch_idx].source_id) );

	sound_buffers[sid].source_id = ch_idx;

	Channels[ch_idx].buf_id = sid;
	Channels[ch_idx].snd_id = snd_id;
	Channels[ch_idx].sig = channel_next_sig++;
	Channels[ch_idx].last_position = 0;
	Channels[ch_idx].is_voice_msg = is_voice_msg;
	Channels[ch_idx].vol = volume;
	Channels[ch_idx].looping = looping;
	Channels[ch_idx].priority = priority;

	if (channel_next_sig < 0)
		channel_next_sig = 1;

	return Channels[ch_idx].sig;
}


// ---------------------------------------------------------------------------------------
// ds_get_channel()
//
// Return the channel number that is playing the sound identified by sig.  If that sound is
// not playing, return -1.
//
int ds_get_channel(int sig)
{
	int i;

	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( Channels[i].source_id && (Channels[i].sig == sig) ) {
			if ( ds_is_channel_playing(i) == TRUE ) {
				return i;
			}
		}
	}

	return -1;
}

// ---------------------------------------------------------------------------------------
// ds_is_channel_playing()
//
//
int ds_is_channel_playing(int channel)
{
	if ( Channels[channel].source_id != 0 ) {
		ALint status;

		OpenAL_ErrorPrint( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &status) );

		return (status == AL_PLAYING);
	}

	return 0;
}

// ---------------------------------------------------------------------------------------
// ds_stop_channel()
//
//
void ds_stop_channel(int channel)
{
	if ( Channels[channel].source_id != 0 ) {
		OpenAL_ErrorPrint( alSourceStop(Channels[channel].source_id) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_stop_channel_all()
//
//	
void ds_stop_channel_all()
{
	int i;

	for ( i=0; i<MAX_CHANNELS; i++ )	{
		if ( Channels[i].source_id != 0 ) {
			OpenAL_ErrorPrint( alSourceStop(Channels[i].source_id) );
		}
	}
}

// ---------------------------------------------------------------------------------------
// ds_set_volume()
//
//	Set the volume for a channel.  The volume is expected to be in DirectSound units
//
//	If the sound is a 3D sound buffer, this is like re-establishing the maximum 
// volume.
//
void ds_set_volume( int channel, int vol )
{
	ALuint source_id = Channels[channel].source_id;

	if (source_id != 0) {
		ALfloat alvol = (vol != -10000) ? powf(10.0f, (float)vol / (-600.0f / log10f(.5f))): 0.0f;

		OpenAL_ErrorPrint( alSourcef(source_id, AL_GAIN, alvol) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_set_pan()
//
//	Set the pan for a channel.  The pan is expected to be in DirectSound units
//
void ds_set_pan( int channel, int pan )
{
	ALint state;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &state), return );

	if (state == AL_PLAYING) {
		ALfloat alpan = (pan != 0) ? ((float)pan / MAX_PAN) : 0.0f;
		OpenAL_ErrorPrint( alSource3f(Channels[channel].source_id, AL_POSITION, alpan, 0.0, 1.0) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_get_pitch()
//
//	Get the pitch of a channel
//
int ds_get_pitch(int channel)
{
	ALint status;
	ALfloat alpitch = 0;
	int pitch;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &status), return -1 );

	if (status == AL_PLAYING)
		OpenAL_ErrorPrint( alGetSourcef(Channels[channel].source_id, AL_PITCH, &alpitch) );

	// convert OpenAL values to DirectSound values and return
	pitch = fl2i( pow(10.0, (alpitch + 2.0)) );

	return pitch;
}

// ---------------------------------------------------------------------------------------
// ds_set_pitch()
//
//	Set the pitch of a channel
//
void ds_set_pitch(int channel, int pitch)
{
	ALint status;

	if ( pitch < MIN_PITCH )
		pitch = MIN_PITCH;

	if ( pitch > MAX_PITCH )
		pitch = MAX_PITCH;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &status), return );

	if (status == AL_PLAYING) {
		ALfloat alpitch = log10f((float)pitch) - 2.0f;
		OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_PITCH, alpitch) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_chg_loop_status()
//
//	
void ds_chg_loop_status(int channel, int loop)
{
	ALuint source_id = Channels[channel].source_id;

	OpenAL_ErrorPrint( alSourcei(source_id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE) );
}

// ---------------------------------------------------------------------------------------
// ds3d_play()
//
// Starts a ds3d sound playing
// 
//	input:
//
//					sid				=>	software id for sound to play
//					hid				=>	hardware id for sound to play (-1 if not in hardware)
//					snd_id			=> identifies what type of sound is playing
//					pos				=>	world pos of sound
//					vel				=>	velocity of object emitting sound
//					min				=>	distance at which sound doesn't get any louder
//					max				=>	distance at which sound becomes inaudible
//					looping			=>	boolean, whether to loop the sound or not
//					max_volume		=>	volume (-10000 to 0) for 3d sound at maximum
//					estimated_vol	=>	manual estimated volume
//					priority		=>		DS_MUST_PLAY
//											DS_LIMIT_ONE
//											DS_LIMIT_TWO
//											DS_LIMIT_THREE
//
//	returns:			0				=> sound started successfully
//						-1				=> sound could not be played
//
int ds3d_play(int sid, int hid, int snd_id, vec3d *pos, vec3d *vel, int min, int max, int looping, int max_volume, int estimated_vol, int priority )
{
	int ch_idx;
	ALfloat alvol = 1.0f, max_vol = 1.0f;
	ALint status;
	
	if (!ds_initialized)
		return -1;

	ch_idx = ds_get_free_channel(estimated_vol, snd_id, priority);

	if (ch_idx < 0) {
	//	nprintf(( "Sound", "SOUND ==> Not playing sound requested at volume %.2f\n", ds_get_percentage_vol(volume) ));
		return -1;
	}


	if (Channels[ch_idx].source_id == 0)
		return -1;
		
//	alDistanceModel(AL_INVERSE_DISTANCE);
		
	// reset pitch value since it could have been changed for this source
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_PITCH, 1.0) );

	// set up 3D sound data here
	ds3d_update_buffer(ch_idx, i2fl(min), i2fl(max), pos, vel);
		
	// Actually play it
	Channels[ch_idx].vol = estimated_vol;
	Channels[ch_idx].looping = looping;
	Channels[ch_idx].priority = priority;

	// set volume
	alvol = (estimated_vol != -10000) ? powf(10.0f, (float)estimated_vol / (-600.0f / log10f(.5f))): 0.0f;
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_GAIN, alvol) );

	// set maximum "inner cone" volume
	max_vol = (max_volume != -10000) ? powf(10.0f, (float)max_volume / (-600.0f / log10f(.5f))): 0.0f;
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_MAX_GAIN, max_vol) );	

	OpenAL_ErrorCheck( alGetSourcei(Channels[ch_idx].source_id, AL_SOURCE_STATE, &status), return -1 );

	if (status == AL_PLAYING)
		OpenAL_ErrorPrint( alSourceStop(Channels[ch_idx].source_id) );

	OpenAL_ErrorCheck( alSourcei(Channels[ch_idx].source_id, AL_BUFFER, sound_buffers[sid].buf_id), return -1 );
	
	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_LOOPING, (looping) ? AL_TRUE : AL_FALSE) );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_SOURCE_RELATIVE, AL_TRUE) );

	OpenAL_ErrorPrint( alSourcePlay(Channels[ch_idx].source_id) );

	sound_buffers[sid].source_id = ch_idx;

	Channels[ch_idx].buf_id = sid;
	Channels[ch_idx].snd_id = snd_id;
	Channels[ch_idx].sig = channel_next_sig++;
	Channels[ch_idx].last_position = 0;

	if (channel_next_sig < 0)
		channel_next_sig = 1;

	return Channels[ch_idx].sig;
}

void ds_set_position(int channel, DWORD offset)
{
#ifdef AL_VERSION_1_1
	OpenAL_ErrorPrint( alSourcei(Channels[channel].source_id, AL_BYTE_OFFSET, offset) );
#endif

//	STUB_FUNCTION;
}

DWORD ds_get_play_position(int channel)
{
	ALint pos = 0;
	int buf_id;

	buf_id = Channels[channel].buf_id;

	if (buf_id == -1)
		return 0;

	if (AL_play_position) {
		OpenAL_ErrorCheck( alGetSourcei( Channels[channel].source_id, AL_BYTE_LOKI, &pos), return 0 );

		if ( pos < 0 ) {
			pos = 0;
		} else if ( pos > 0 ) {
			// AL_BYTE_LOKI returns position in canon format which may differ
			// from our sample, so we may have to scale it
			ALuint buf = sound_buffers[buf_id].buf_id;
			ALint size;

			OpenAL_ErrorCheck( alGetBufferi(buf, AL_SIZE, &size), return 0 );

			pos = (ALint)(pos * ((float)sound_buffers[buf_id].nbytes / size));
		}
	}
#ifdef AL_VERSION_1_1
	// AL_play_position should only be available under Linux, but OpenAL 1.1 provides a standard way now (except under Linux :()
	else {
		OpenAL_ErrorCheck( alGetSourcei( Channels[channel].source_id, AL_BYTE_OFFSET, &pos), return 0 );

		if ( pos < 0 )
			pos = 0;
	}
#endif

	return pos;
}

DWORD ds_get_write_position(int channel)
{
//	STUB_FUNCTION;

	return 0;
}

int ds_get_channel_size(int channel)
{
	int buf_id = Channels[channel].buf_id;

	if (buf_id != -1) {
		return sound_buffers[buf_id].nbytes;
	}

	return 0;
}

// Returns the number of channels that are actually playing
int ds_get_number_channels()
{
	int i,n;

	if (!ds_initialized) {
		return 0;
	}

	n = 0;
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( Channels[i].source_id ) {
			if ( ds_is_channel_playing(i) == TRUE ) {
				n++;
			}
		}
	}

	return n;
}

// retreive raw data from a sound buffer
int ds_get_data(int sid, char *data)
{
	STUB_FUNCTION;

	return -1;
}

// return the size of the raw sound data
int ds_get_size(int sid, int *size)
{
	Assert(sid >= 0);

	STUB_FUNCTION;

	return -1;
}

int ds_using_ds3d()
{
	return Ds_use_ds3d;
}

// Return the primary buffer interface.  Note that we cast to a uint to avoid
// having to include dsound.h (and thus windows.h) in ds.h.
//
uint ds_get_primary_buffer_interface()
{
	// unused
	return 0;
}

// Return the DirectSound Interface.
//
uint ds_get_dsound_interface()
{
	// unused
	return 0;
}

uint ds_get_property_set_interface()
{
	return 0;
}

// --------------------
//
// EAX Functions below
//
// --------------------

// Set the master volume for the reverb added to all sound sources.
//
// volume: volume, range from 0 to 1.0
//
// returns: 0 if the volume is set successfully, otherwise return -1
//
int ds_eax_set_volume(float volume)
{
	return -1;
}

// Set the decay time for the EAX environment (ie all sound sources)
//
// seconds: decay time in seconds
//
// returns: 0 if decay time is successfully set, otherwise return -1
//
int ds_eax_set_decay_time(float seconds)
{
	return -1;
}

// Set the damping value for the EAX environment (ie all sound sources)
//
// damp: damp value from 0 to 2.0
//
// returns: 0 if the damp value is successfully set, otherwise return -1
//
int ds_eax_set_damping(float damp)
{
	return -1;
}

// Set up the environment type for all sound sources.
//
// envid: value from the EAX_ENVIRONMENT_* enumeration in ds_eax.h
//
// returns: 0 if the environment is set successfully, otherwise return -1
//
int ds_eax_set_environment(unsigned long envid)
{
	return -1;
}

// Set up a predefined environment for EAX
//
// envid: value from teh EAX_ENVIRONMENT_* enumeration
//
// returns: 0 if successful, otherwise return -1
//
int ds_eax_set_preset(unsigned long envid)
{
	return -1;
}


// Set up all the parameters for an environment
//
// id: value from teh EAX_ENVIRONMENT_* enumeration
// volume: volume for the environment (0 to 1.0)
// damping: damp value for the environment (0 to 2.0)
// decay: decay time in seconds (0.1 to 20.0)
//
// returns: 0 if successful, otherwise return -1
//
int ds_eax_set_all(unsigned long id, float vol, float damping, float decay)
{
	return -1;
}

// Get up the parameters for the current environment
//
// er: (output) hold environment parameters
//
// returns: 0 if successful, otherwise return -1
//
int ds_eax_get_all(EAX_REVERBPROPERTIES *er)
{
	return -1;
}

// Close down EAX, freeing any allocated resources
//
void ds_eax_close()
{
}

// Initialize EAX
//
// returns: 0 if initialization is successful, otherwise return -1
//
int ds_eax_init()
{
	return -1;
}

int ds_eax_is_inited()
{
	return 0;
}

bool ds_using_a3d()
{
	return false;
}

// Called once per game frame to make sure voice messages aren't looping
//
void ds_do_frame()
{
	if (!ds_initialized)
		return;

	int i;
	channel *cp = NULL;

	for (i = 0; i < MAX_CHANNELS; i++) {
		cp = &Channels[i];
		Assert( cp != NULL );

		if (cp->is_voice_msg == true) {
			if( cp->source_id == 0 ) {
				continue;
			}

			DWORD current_position = ds_get_play_position(i);
			if (current_position != 0) {
				if (current_position < cp->last_position) {
					ds_close_channel(i);
				} else {
					cp->last_position = current_position;
				}
			}
		}
	}
}

// given a valid channel return the sound id
int ds_get_sound_id(int channel)
{
	Assert( channel >= 0 );

	return Channels[channel].snd_id;
}


#ifdef SCP_UNIX
void dscap_close()
{
	STUB_FUNCTION;
}

int dscap_create_buffer(int freq, int bits_per_sample, int nchannels, int nseconds)
{
//	STUB_FUNCTION;

	return -1;
}

int dscap_get_raw_data(unsigned char *outbuf, unsigned int max_size)
{
//	STUB_FUNCTION;

	return -1;
}

int dscap_max_buffersize()
{
//	STUB_FUNCTION;
	
	return -1;
}

void dscap_release_buffer()
{
//	STUB_FUNCTION;
}

int dscap_start_record()
{
//	STUB_FUNCTION;

	return -1;
}

int dscap_stop_record()
{
//	STUB_FUNCTION;

	return -1;
}

int dscap_supported()
{
//	STUB_FUNCTION;

	return 0;
}
#endif
