#ifndef _MSC_VER
#include <stdint.h>
#endif
#include <sys/types.h>

#include "quakedef.h"
#include "client.h"
#include "cap_lavc.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/pixfmt.h>

qboolean SCR_CaptureVideo_Lavc_OpenLibrary(void)
{
	return 1;
}

void SCR_CaptureVideo_Lavc_Init(void)
{
}

qboolean SCR_CaptureVideo_Lavc_Available(void)
{
	return 1;
}

void SCR_CaptureVideo_Lavc_CloseDLL(void)
{
}

typedef struct capturevideostate_lavc_formatspecific_s
{
	AVFormatContext *avf;
	int apts;
	int vpts;
	unsigned char *buffer;
	size_t bufsize;
}
capturevideostate_lavc_formatspecific_t;
#define LOAD_FORMATSPECIFIC_LAVC() capturevideostate_lavc_formatspecific_t *format = (capturevideostate_lavc_formatspecific_t *) cls.capturevideo.formatspecific

static void SCR_CaptureVideo_Lavc_EndVideo(void)
{
	LOAD_FORMATSPECIFIC_LAVC();

	av_write_trailer(format->avf);
	{
		unsigned int i;
		for (i = 0; i < format->avf->nb_streams; i++) {
			avcodec_close(format->avf->streams[i]->codec);
			av_free(format->avf->streams[i]->codec);
			av_free(format->avf->streams[i]->info);
			av_free(format->avf->streams[i]);
		}
		av_free(format->avf);
		format->avf = NULL;
	}
	Mem_Free(format);

	FS_Close(cls.capturevideo.videofile);
	cls.capturevideo.videofile = NULL;
}

static void SCR_CaptureVideo_Lavc_VideoFrames(int num)
{
	LOAD_FORMATSPECIFIC_LAVC();

	// data is in cls.capturevideo.outbuffer as BGRA and has size width*height
	AVCodecContext *avc = format->avf->streams[0]->codec;
	AVFrame frame;
	int size;

	avcodec_get_frame_defaults(&frame);
	frame.data[0] = cls.capturevideo.outbuffer;
	frame.linesize[0] = 4*cls.capturevideo.width;
	frame.pts = format->vpts;
	size = avcodec_encode_video(avc, format->buffer, format->bufsize, &frame);
	if(size < 0)
		Con_Printf("error encoding\n");
	if(size > 0)
	{
		AVPacket packet;
		av_init_packet(&packet);
		packet.stream_index = 0;
		packet.data = format->buffer;
		packet.size = size;
		packet.pts = format->vpts;
		if(av_interleaved_write_frame(format->avf, &packet) < 0)
			Con_Printf("error writing\n");
	}

	format->vpts += num;
}

typedef int channelmapping_t[8];
static channelmapping_t mapping[8] =
{
	{ 0, -1, -1, -1, -1, -1, -1, -1 }, // mono
	{ 0, 1, -1, -1, -1, -1, -1, -1 }, // stereo
	{ 0, 1, 2, -1, -1, -1, -1, -1 }, // L C R
	{ 0, 1, 2, 3, -1, -1, -1, -1 }, // surround40
	{ 0, 2, 3, 4, 1, -1, -1, -1 }, // FL FC FR RL RR
	{ 0, 2, 3, 4, 1, 5, -1, -1 }, // surround51
	{ 0, 2, 3, 4, 1, 5, 6, -1 }, // (not defined by vorbis spec)
	{ 0, 2, 3, 4, 1, 5, 6, 7 } // surround71 (not defined by vorbis spec)
};

static void SCR_CaptureVideo_Lavc_SoundFrame(const portable_sampleframe_t *paintbuffer, size_t length)
{
	LOAD_FORMATSPECIFIC_LAVC();

#if 0
	if(cls.capturevideo.soundrate)
	{
		AVCodecContext *avc = format->avf->streams[1]->codec;
		int size;
		size_t i;
		int *map = mapping[bound(1, cls.capturevideo.soundchannels, 8) - 1];

		short *shortpaintbuffer = Z_Malloc(length * cls.capturevideo.soundchannels * sizeof(short));
		for(i = 0; i < length; ++i)
			for(j = 0; j < cls.capturevideo.soundchannels; ++j)
				shortpaintbuffer[i*cls.capturevideo.soundchannels+map[j]] = paintbuffer[i].sample[j];

		for(;;)
		{
			size = avcodec_encode_audio(avc, format->buffer, format->bufsize, shortpaintbuffer);
			if(size < 0)
				Con_Printf("error encoding\n");
			if(size > 0)
			{
				AVPacket packet;
				av_init_packet(&packet);
				packet.stream_index = 0;
				packet.data = format->buffer;
				packet.size = size;
				packet.pts = format->vpts;
				if(av_interleaved_write_frame(format->avf, &packet) < 0)
					Con_Printf("error writing\n");
			}
		}
	}
#endif
}

void SCR_CaptureVideo_Lavc_BeginVideo(void)
{
	cls.capturevideo.format = CAPTUREVIDEOFORMAT_LAVC;
	cls.capturevideo.formatextension = "ogv";
	cls.capturevideo.videofile = FS_OpenRealFile(va("%s.%s", cls.capturevideo.basename, cls.capturevideo.formatextension), "wb", false);
	cls.capturevideo.endvideo = SCR_CaptureVideo_Lavc_EndVideo;
	cls.capturevideo.videoframes = SCR_CaptureVideo_Lavc_VideoFrames;
	cls.capturevideo.soundframe = SCR_CaptureVideo_Lavc_SoundFrame;
	cls.capturevideo.formatspecific = Mem_Alloc(tempmempool, sizeof(capturevideostate_lavc_formatspecific_t));
	{
		LOAD_FORMATSPECIFIC_LAVC();
		AVStream *video_str;
		AVCodec *encoder;
		int num, denom;

		format->avf = avformat_alloc_context();
		format->avf->oformat = av_guess_format("avi", va("%s.%s", cls.capturevideo.basename, cls.capturevideo.formatextension), NULL);
		strlcpy(format->avf->filename, "/tmp/foo.avi", sizeof(format->avf->filename)); // TODO use the real file

		video_str = av_new_stream(format->avf, 0);
		video_str->codec->codec_type = AVMEDIA_TYPE_VIDEO;
		video_str->codec->codec_id = CODEC_ID_MPEG4;

		FindFraction(cls.capturevideo.framerate / cls.capturevideo.framestep, &num, &denom, 1001);
		video_str->codec->time_base.num = denom;
		video_str->codec->time_base.den = num;

		video_str->codec->width = cls.capturevideo.width;
		video_str->codec->height = cls.capturevideo.height;
		video_str->codec->pix_fmt = PIX_FMT_BGR32_1;

		FindFraction(1 / vid_pixelheight.value, &num, &denom, 1000);
		video_str->sample_aspect_ratio.num = num;
		video_str->sample_aspect_ratio.den = denom;

		if(format->avf->oformat->flags & AVFMT_GLOBALHEADER)
			video_str->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

		encoder = avcodec_find_encoder(video_str->codec->codec_id);
		avcodec_open(video_str->codec, encoder);

		format->vpts = 0;

#if 0
		if(cls.capturevideo.soundrate)
		{
			AVStream *audio_str;
			audio_str = av_new_stream(format->avf, 0);
			audio_str->codec->codec_type = AVMEDIA_TYPE_AUDIO;
			audio_str->codec->codec_id = CODEC_ID_PCM_S16LE;

			audio_str->codec->time_base.num = 1;
			audio_str->codec->time_base.den = cls.capturevideo.soundrate;

			audio_str->codec->sample_rate = cls.capturevideo.soundrate;
			audio_str->codec->channels = cls.capturevideo.soundchannels;
			audio_str->codec->sample_fmt = AV_SAMPLE_FMT_S16;

			if(format->avf->oformat->flags & AVFMT_GLOBALHEADER)
				audio_str->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

			encoder = avcodec_find_encoder(audio_str->codec->codec_id);
			avcodec_open(audio_str->codec, encoder);

			format->apts = 0;
		}
#endif

		url_fopen(&format->avf->pb, format->avf->filename, URL_WRONLY); // FIXME
		av_write_header(format->avf);

		format->bufsize = cls.capturevideo.width * cls.capturevideo.height * 6 + 200;
		format->buffer = Z_Malloc(format->bufsize);
	}
}
