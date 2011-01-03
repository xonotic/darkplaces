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
	avcodec_register_all();
	av_register_all();
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
	unsigned char *yuv;
	unsigned char *buffer;
	size_t bufsize;
	short *aframe;
	int aframepos;
}
capturevideostate_lavc_formatspecific_t;
#define LOAD_FORMATSPECIFIC_LAVC() capturevideostate_lavc_formatspecific_t *format = (capturevideostate_lavc_formatspecific_t *) cls.capturevideo.formatspecific

static void SCR_CaptureVideo_Lavc_EndVideo(void)
{
	LOAD_FORMATSPECIFIC_LAVC();

	if(format->buffer)
	{
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
	}
	Mem_Free(format);

	FS_Close(cls.capturevideo.videofile);
	cls.capturevideo.videofile = NULL;
}

static void SCR_CaptureVideo_Lavc_ConvertFrame_BGRA_to_YUV(AVFrame *frame)
{
	LOAD_FORMATSPECIFIC_LAVC();
	int x, y;
	int blockr, blockg, blockb;
	unsigned char *b = cls.capturevideo.outbuffer;
	int w = cls.capturevideo.width;
	int uw = (w+1)/2;
	int h = cls.capturevideo.height;
	int uh = (h+1)/2;
	int inpitch = w*4;

	unsigned char *yuvy = format->yuv;
	unsigned char *yuvu = yuvy + w*h;
	unsigned char *yuvv = yuvu + uw*uh;

	frame->data[0] = yuvy;
	frame->linesize[0] = w;
	frame->data[1] = yuvu;
	frame->linesize[1] = uw;
	frame->data[2] = yuvv;
	frame->linesize[2] = uw;

	for(y = 0; y < h; ++y)
	{
		for(b = cls.capturevideo.outbuffer + (h-1-y)*w*4, x = 0; x < w; ++x)
		{
			blockr = b[2];
			blockg = b[1];
			blockb = b[0];
			yuvy[x + w * y] =
				cls.capturevideo.yuvnormalizetable[0][cls.capturevideo.rgbtoyuvscaletable[0][0][blockr] + cls.capturevideo.rgbtoyuvscaletable[0][1][blockg] + cls.capturevideo.rgbtoyuvscaletable[0][2][blockb]];
			b += 4;
		}

		if((y & 1) == 0)
		{
			for(b = cls.capturevideo.outbuffer + (h-2-y)*w*4, x = 0; x < (w+1)/2; ++x)
			{
				blockr = (b[2] + b[6] + b[inpitch+2] + b[inpitch+6]) >> 2;
				blockg = (b[1] + b[5] + b[inpitch+1] + b[inpitch+5]) >> 2;
				blockb = (b[0] + b[4] + b[inpitch+0] + b[inpitch+4]) >> 2;
				yuvu[x + uw * (y/2)] =
					cls.capturevideo.yuvnormalizetable[1][cls.capturevideo.rgbtoyuvscaletable[1][0][blockr] + cls.capturevideo.rgbtoyuvscaletable[1][1][blockg] + cls.capturevideo.rgbtoyuvscaletable[1][2][blockb] + 128];
				yuvv[x + uw * (y/2)] =
					cls.capturevideo.yuvnormalizetable[2][cls.capturevideo.rgbtoyuvscaletable[2][0][blockr] + cls.capturevideo.rgbtoyuvscaletable[2][1][blockg] + cls.capturevideo.rgbtoyuvscaletable[2][2][blockb] + 128];
				b += 8;
			}
		}
	}
}


static void SCR_CaptureVideo_Lavc_VideoFrames(int num)
{
	LOAD_FORMATSPECIFIC_LAVC();

	// data is in cls.capturevideo.outbuffer as BGRA and has size width*height
	AVCodecContext *avc = format->avf->streams[0]->codec;
	AVFrame frame;
	int size;

	avcodec_get_frame_defaults(&frame);
	SCR_CaptureVideo_Lavc_ConvertFrame_BGRA_to_YUV(&frame);
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

// FIXME find the correct mappings for DP to ffmpeg
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

	if(cls.capturevideo.soundrate)
	{
		AVCodecContext *avc = format->avf->streams[1]->codec;
		int size;
		size_t i;
		int *map = mapping[bound(1, cls.capturevideo.soundchannels, 8) - 1];
		size_t bufpos = 0;

		// FIXME encode the rest of the buffer at the end of the video, filled with zeroes!
		while(bufpos < length)
		{
			// fill up buffer
			while(bufpos < length && format->aframepos < avc->frame_size)
			{
				for(i = 0; i < cls.capturevideo.soundchannels; ++i)
					format->aframe[format->aframepos*cls.capturevideo.soundchannels+map[i]] = paintbuffer[bufpos].sample[i];
				++bufpos;
				++format->aframepos;
			}

			if(format->aframepos >= avc->frame_size)
			{
				size = avcodec_encode_audio(avc, format->buffer, format->bufsize, format->aframe);
				if(size < 0)
					Con_Printf("error encoding\n");
				if(size > 0)
				{
					AVPacket packet;
					av_init_packet(&packet);
					packet.stream_index = 1;
					packet.data = format->buffer;
					packet.size = size;
					packet.pts = format->apts;
					if(av_interleaved_write_frame(format->avf, &packet) < 0)
						Con_Printf("error writing\n");
				}

				format->apts += avc->frame_size;
				format->aframepos = 0;
			}
			else
			{
				// if we get here, frame_size was not hit
				// this means that length has been hit!
				break;
			}
		}
	}
}

// TODO error checking in this function
// TODO parameters in this function
void SCR_CaptureVideo_Lavc_BeginVideo(void)
{
	cls.capturevideo.format = CAPTUREVIDEOFORMAT_LAVC;
	cls.capturevideo.formatextension = "avi";
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
		if(!format->avf->oformat)
		{
			Con_Printf("Failed to find format\n");
			SCR_CaptureVideo_EndVideo();
			return;
		}
		strlcpy(format->avf->filename, "/tmp/foo.avi", sizeof(format->avf->filename)); // TODO use the qfile_t

		video_str = av_new_stream(format->avf, 0);
		video_str->codec->codec_type = AVMEDIA_TYPE_VIDEO;
		video_str->codec->codec_id = CODEC_ID_MPEG4;

		FindFraction(cls.capturevideo.framerate / cls.capturevideo.framestep, &num, &denom, 1001);
		video_str->codec->time_base.num = denom;
		video_str->codec->time_base.den = num;

		video_str->codec->width = cls.capturevideo.width;
		video_str->codec->height = cls.capturevideo.height;
		video_str->codec->pix_fmt = PIX_FMT_YUV420P;

		FindFraction(1 / vid_pixelheight.value, &num, &denom, 1000);
		video_str->sample_aspect_ratio.num = num;
		video_str->sample_aspect_ratio.den = denom;
		video_str->codec->sample_aspect_ratio.num = num;
		video_str->codec->sample_aspect_ratio.den = denom;

		if(format->avf->oformat->flags & AVFMT_GLOBALHEADER)
			video_str->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

		encoder = avcodec_find_encoder(video_str->codec->codec_id);
		if(!encoder)
		{
			Con_Printf("Failed to find encoder\n");
			SCR_CaptureVideo_EndVideo();
			return;
		}
		if(avcodec_open(video_str->codec, encoder) < 0)
		{
			Con_Printf("Failed to open encoder\n");
			SCR_CaptureVideo_EndVideo();
			return;
		}

		format->vpts = 0;
		format->bufsize = cls.capturevideo.width * cls.capturevideo.height * 6 + 200;

		if(cls.capturevideo.soundrate)
		{
			AVStream *audio_str;
			audio_str = av_new_stream(format->avf, 0);
			audio_str->codec->codec_type = AVMEDIA_TYPE_AUDIO;
			//audio_str->codec->codec_id = CODEC_ID_PCM_S16LE;
			audio_str->codec->codec_id = CODEC_ID_MP3;

			audio_str->codec->time_base.num = 1;
			audio_str->codec->time_base.den = cls.capturevideo.soundrate;

			audio_str->codec->sample_rate = cls.capturevideo.soundrate;
			audio_str->codec->channels = cls.capturevideo.soundchannels;
			audio_str->codec->sample_fmt = AV_SAMPLE_FMT_S16;

			if(format->avf->oformat->flags & AVFMT_GLOBALHEADER)
				audio_str->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

			encoder = avcodec_find_encoder(audio_str->codec->codec_id);
			if(!encoder)
			{
				Con_Printf("Failed to find encoder\n");
				SCR_CaptureVideo_EndVideo();
				return;
			}
			if(avcodec_open(audio_str->codec, encoder) < 0)
			{
				Con_Printf("Failed to open encoder\n");
				SCR_CaptureVideo_EndVideo();
				return;
			}

			format->apts = 0;
			format->aframe = Z_Malloc(audio_str->codec->frame_size * sizeof(*format->aframe) * cls.capturevideo.soundchannels);
			format->aframepos = 0;

			format->bufsize = max(format->bufsize, audio_str->codec->frame_size * sizeof(*format->aframe) * cls.capturevideo.soundchannels * 2 + 200);
		}

		url_fopen(&format->avf->pb, format->avf->filename, URL_WRONLY); // FIXME use the qfile_t
		av_write_header(format->avf);

		format->buffer = Z_Malloc(format->bufsize);
		format->yuv = Z_Malloc(cls.capturevideo.width * cls.capturevideo.height + ((cls.capturevideo.width + 1) / 2) * ((cls.capturevideo.height + 1) / 2) * 2);
	}
}
