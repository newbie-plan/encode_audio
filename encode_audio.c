#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

static void encode(AVCodecContext *cdc_ctx, AVFrame *frame, AVPacket *pkt, FILE *fp_out)
{
	int ret = 0;

	if ((ret = avcodec_send_frame(cdc_ctx, frame)) < 0)
	{
		fprintf(stderr, "avcodec_send_frame failed.\n");
		exit(1);
	}

	while ((ret = avcodec_receive_packet(cdc_ctx, pkt)) >= 0)
	{
		printf("Write (size=%d) packet.\n", pkt->size);
		fwrite(pkt->data, 1, pkt->size, fp_out);
		av_packet_unref(pkt);
	}

	if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF))
	{
		fprintf(stderr, "avcodec_receive_packet failed.\n");
		exit(1);
	}
}

void encode_audio(const char *input_file, const char *output_file)
{
	int ret = 0;
	int data_size = 0;
	AVCodec *codec = NULL;
	AVCodecContext *cdc_ctx = NULL;
	AVPacket *pkt = NULL;
	AVFrame *frame = NULL;
	FILE *fp_in, *fp_out;

	if ((codec = avcodec_find_encoder(AV_CODEC_ID_MP3)) == NULL)
	{
		fprintf(stderr, "avcodec_find_encoder_by_name failed.\n");
		goto ret1;
	}

	if ((cdc_ctx = avcodec_alloc_context3(codec)) == NULL)
	{
		fprintf(stderr, "avcodec_alloc_context3 failed.\n");
		goto ret1;
	}

#if 1 	/*encode zhu.pcm*/
	cdc_ctx->bit_rate = 192000;
	cdc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
	cdc_ctx->sample_rate = 44100;
	cdc_ctx->channel_layout = av_get_channel_layout("stereo");
	cdc_ctx->channels = av_get_channel_layout_nb_channels(cdc_ctx->channel_layout);

#else 	/*encode 16k.pcm*/
	cdc_ctx->bit_rate = 64000;
	cdc_ctx->sample_fmt = AV_SAMPLE_FMT_S16P;
	cdc_ctx->sample_rate = 16000;
	cdc_ctx->channel_layout = av_get_channel_layout("mono");
	cdc_ctx->channels = av_get_channel_layout_nb_channels(cdc_ctx->channel_layout);
#endif

	if ((ret = avcodec_open2(cdc_ctx, codec, NULL)) < 0)
	{
		fprintf(stderr, "avcodec_open2 failed.\n");
		goto ret2;
	}

	if ((pkt = av_packet_alloc()) == NULL)
	{
		fprintf(stderr, "av_packet_alloc failed.\n");
		goto ret3;
	}

	if ((frame = av_frame_alloc()) == NULL)
	{
		fprintf(stderr, "av_frame_alloc failed.\n");
		goto ret4;
	}
	frame->nb_samples = cdc_ctx->frame_size;
	frame->format = cdc_ctx->sample_fmt;
	frame->channel_layout = cdc_ctx->channel_layout;

	if ((ret = av_frame_get_buffer(frame, 0)) < 0)
	{
		fprintf(stderr, "av_frame_get_buffer failed.\n");
		goto ret5;
	}

	if ((fp_in = fopen(input_file, "rb")) == NULL)
	{
		fprintf(stderr, "fopen %s failed.\n", input_file);
		goto ret5;
	}
	if ((fp_out = fopen(output_file, "wb")) == NULL)
	{
		fprintf(stderr, "fopen %s failed.\n", output_file);
		goto ret6;
	}

#if 1 		/*cdc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP*/
	data_size = av_get_bytes_per_sample(cdc_ctx->sample_fmt);

	while (feof(fp_in) == 0)
	{
		int i = 0, ch = 0;

		if ((ret = av_frame_make_writable(frame)) < 0)
		{
			fprintf(stderr, "frame is not writable.\n");
			goto ret7;
		}

		for (i = 0; i < frame->nb_samples; i++)
		{
			for (ch = 0; ch < cdc_ctx->channels; ch++)
			{
				fread(frame->data[ch] + data_size * i, 1, data_size, fp_in);
			}
		}

		encode(cdc_ctx, frame, pkt, fp_out);
	}

#else 		/*cdc_ctx->sample_fmt = AV_SAMPLE_FMT_S16P*/
	data_size = av_samples_get_buffer_size(NULL, cdc_ctx->channels, cdc_ctx->frame_size, cdc_ctx->sample_fmt, 1);
	printf("data_size = %d\n", data_size);

	while (feof(fp_in) == 0)
	{
		if ((ret = av_frame_make_writable(frame)) < 0)
		{
			fprintf(stderr, "frame is not writable.\n");
			goto ret7;
		}
		
		fread(frame->data[0], 1, data_size, fp_in);

		encode(cdc_ctx, frame, pkt, fp_out);
	}
#endif

	encode(cdc_ctx, NULL, pkt, fp_out);


	fclose(fp_out);
	fclose(fp_in);
	av_frame_free(&frame);
	av_packet_free(&pkt);
	avcodec_close(cdc_ctx);
	avcodec_free_context(&cdc_ctx);
	return;
ret7:
	fclose(fp_out);
ret6:
	fclose(fp_in);
ret5:
	av_frame_free(&frame);
ret4:
	av_packet_free(&pkt);
ret3:
	avcodec_close(cdc_ctx);
ret2:
	avcodec_free_context(&cdc_ctx);
ret1:
	exit(1);
}

int main(int argc, const char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "Uage:<input file> <output file>\n");
		exit(0);
	}

	encode_audio(argv[1], argv[2]);
	
	return 0;
}
