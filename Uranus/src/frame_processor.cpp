#include "frame_processor.hpp"

FrameProcessor* FrameProcessor::frame_processor_ = nullptr;

FrameProcessor::FrameProcessor() {

	// ʹ��H264������
	codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);

	// ���ý�����������
	codec_context_ = avcodec_alloc_context3(codec_);

	if (codec_->capabilities & AV_CODEC_CAP_TRUNCATED)
		codec_context_->flags |= AV_CODEC_CAP_TRUNCATED;

	avcodec_open2(codec_context_, codec_, nullptr);

	// ʹ��H264��Ƶ��������
	parser_context_ = av_parser_init(AV_CODEC_ID_H264);

	// ����YUV֡�ڴ�ռ�
	frame_ = av_frame_alloc();

	// ����֡���ݰ��ڴ�ռ�
	packet_ = new AVPacket;

	// ֡���ݰ���ʼ��
	av_init_packet(packet_);

	// ����YUV-RGB�任������
	sws_context_ = nullptr;

	// ����RGB֡�ڴ�ռ�
	rgb_frame_ = av_frame_alloc();
}

FrameProcessor::~FrameProcessor() {
	av_free(frame_);
	av_free(rgb_frame_);
	avcodec_close(codec_context_);
}

FrameProcessor* FrameProcessor::GetInstance() {
	if (frame_processor_ == nullptr)
		frame_processor_ = new FrameProcessor();
	return frame_processor_;
}

void FrameProcessor::ConsturctFrame(QByteArray& bytes) {
	auto length = bytes.length();
	auto data_in = new uchar[length];
	auto *data_in_ptr = data_in;

	// ���ﲻʹ��ǿ������ת��������char���͵�ǿ������ת������\0ֱ��ֹͣ�����¶�ʧ����
	memcpy(data_in, bytes.data(), length);

	while (length > 0) {
		const size_t num_consumed = av_parser_parse2(parser_context_, codec_context_, &packet_->data, &packet_->size,
													 data_in, length, 0, 0, AV_NOPTS_VALUE);

		const auto is_frame_available = packet_->size > 0;

		if (is_frame_available) {
			if (avcodec_send_packet(codec_context_, packet_) == 0 && avcodec_receive_frame(codec_context_, frame_) == 0) {
				const auto width = frame_->width;
				const auto height = frame_->height;
				const auto out_buffer = static_cast<uint8_t *>(av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32, width, height, 1) * sizeof(uint8_t)));

				sws_context_ = sws_getCachedContext(sws_context_, width, height, static_cast<AVPixelFormat>(frame_->format),
													width, height, AV_PIX_FMT_RGB32, SWS_BILINEAR, nullptr, nullptr, nullptr);

				av_image_fill_arrays(rgb_frame_->data, rgb_frame_->linesize, out_buffer, AV_PIX_FMT_RGB32, width, height, 1);

				sws_scale(sws_context_, frame_->data, frame_->linesize, 0, height,
						  rgb_frame_->data, rgb_frame_->linesize);
				rgb_frame_->width = width;
				rgb_frame_->height = height;

				const auto temp_image = QImage(static_cast<uchar*>(out_buffer), width, height, QImage::Format_RGB32);
				const auto image = temp_image.copy();
				av_free(out_buffer);

				emit frame_ready_signal(image);
			}
		}
		length -= num_consumed;
		data_in += num_consumed;
	}

	data_in = data_in_ptr;
	delete[] data_in;
	av_packet_unref(packet_);
}