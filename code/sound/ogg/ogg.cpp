

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Mmsystem.h>
#endif

#include "cfile/cfile.h"
#include "sound/ogg/ogg.h"
#include "sound/audiostr.h"


int ogg_inited = 0;
ov_callbacks cfile_callbacks;
ov_callbacks mmio_callbacks;

//Encapsulation funcs to please the almighty ov_callbacks struct
size_t ogg_cfread(void *buf, size_t elsize, size_t elnem, void* cfile)
{
	return cfread(buf, elsize, elnem, (CFILE*)cfile);
}

int ogg_cfseek(void* cfile, ogg_int64_t offset, int where)
{
	return cfseek((CFILE*)cfile, (int) offset, where);
}

int ogg_cfclose(void* cfile)
{
	// we don't close here so that it's safe to do it ourselves
	return 0;
}

long ogg_cftell(void* cfile)
{
	return cftell((CFILE*) cfile);
}


size_t ogg_mmio_read(void *buf, size_t elsize, size_t elnem, void* mmfp)
{
	audio_spec_t *hdl = (audio_spec_t*)mmfp;

	return SDL_RWread(hdl->cfp, (char *) buf, 1, elsize * elnem);
}

int ogg_mmio_seek(void* mmfp, ogg_int64_t offset, int where)
{
	audio_spec_t *hdl = (audio_spec_t*)mmfp;

	long rc = 0, cur_offset = 0;

	switch (where)
	{
		case SEEK_CUR:
		{
			cur_offset = SDL_RWseek(hdl->cfp, 0, RW_SEEK_CUR);

			if ((cur_offset + offset) > SDL_RWsize(hdl->cfp))
				return -1;

			rc = SDL_RWseek(hdl->cfp, cur_offset + (long)offset, RW_SEEK_SET);

			break;
		}

		case SEEK_SET:
		{
			if (offset > SDL_RWsize(hdl->cfp))
				return -1;

			rc = SDL_RWseek(hdl->cfp, (long)offset, RW_SEEK_SET);

			break;
		}

		case SEEK_END:
		{
			rc = SDL_RWseek(hdl->cfp,  SDL_RWsize(hdl->cfp), RW_SEEK_SET);

			break;
		}
	}

	if (rc < 0)
		return -1;

	return (int)rc;
}

int ogg_mmio_close(void* mmfp)
{
	// we don't close here so that it's safe to do it ourselves
	return 0;
}

long ogg_mmio_tell(void* mmfp)
{
	audio_spec_t *hdl = (audio_spec_t*)mmfp;
	return SDL_RWtell(hdl->cfp);
}

int OGG_init()
{
	//Setup the cfile_callbacks stucts
	cfile_callbacks.read_func = ogg_cfread;
	cfile_callbacks.seek_func = ogg_cfseek;
	cfile_callbacks.close_func = ogg_cfclose;
	cfile_callbacks.tell_func = ogg_cftell;

	mmio_callbacks.read_func = ogg_mmio_read;
	mmio_callbacks.seek_func = ogg_mmio_seek;
	mmio_callbacks.close_func= ogg_mmio_close;
	mmio_callbacks.tell_func = ogg_mmio_tell;

	return 0;
}
