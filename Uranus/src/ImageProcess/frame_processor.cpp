#include "frame_processor.hpp"

FrameProcessor* FrameProcessor::frame_processor_ = nullptr;

FrameProcessor* FrameProcessor::GetInstance() {
	if (frame_processor_ == nullptr)
		frame_processor_ = new FrameProcessor();
	return frame_processor_;
}

FrameProcessor::FrameProcessor() {

	// 目标跟踪
	is_ready_to_select_target_ = false;
	is_ready_to_track_target_ = false;
	target_tracker_ = TargetTracker::GetInstance();

	// 使用H264解码器
	codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);

	// 设置解码器上下文
	codec_context_ = avcodec_alloc_context3(codec_);

	if (codec_->capabilities & AV_CODEC_CAP_TRUNCATED)
		codec_context_->flags |= AV_CODEC_CAP_TRUNCATED;

	avcodec_open2(codec_context_, codec_, nullptr);

	// 使用H264视频流处理器
	parser_context_ = av_parser_init(AV_CODEC_ID_H264);

	// 分配YUV帧内存空间
	frame_ = av_frame_alloc();

	// 分配帧数据包内存空间
	packet_ = new AVPacket;

	// 帧数据包初始化
	av_init_packet(packet_);

	// 设置YUV-RGB变换上下文
	sws_context_ = nullptr;

	// 分配RGB帧内存空间
	rgb_frame_ = av_frame_alloc();
}

void FrameProcessor::ConsturctFrame(QByteArray& bytes) {
	auto length = bytes.length();
	auto data_in = new uchar[length];
	auto *data_in_ptr = data_in;

	// 这里不使用强制类型转换，否则char类型的强制类型转换遇到\0直接停止，导致丢失数据
	memcpy(data_in, bytes.data(), length);

	while (length > 0) {
		// 将一段数据包中解析出一帧的数据，填充到packet_->data中
		const size_t num_consumed = av_parser_parse2(parser_context_, codec_context_, &packet_->data, &packet_->size,
													 data_in, length, 0, 0, AV_NOPTS_VALUE);

		// 填充之后的packet_size大于0则表明成功解析出一帧的数据
		const auto is_frame_available = packet_->size > 0;

		if (is_frame_available) {
			// 从一帧h264裸流数据中解码成YUV格式的图像
			if (avcodec_send_packet(codec_context_, packet_) == 0 && avcodec_receive_frame(codec_context_, frame_) == 0) {
				const auto width = frame_->width;
				const auto height = frame_->height;

				// 分配用于生成帧的Buffer空间
				const auto out_buffer = static_cast<uint8_t *>(av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1) * sizeof(uint8_t)));

				// 需要获得第一帧才能得知上下文
				sws_context_ = sws_getCachedContext(sws_context_, width, height, static_cast<AVPixelFormat>(frame_->format),
													width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

				// 将YUV格式转换成RGB格式
				av_image_fill_arrays(rgb_frame_->data, rgb_frame_->linesize, out_buffer, AV_PIX_FMT_RGB24, width, height, 1);

				sws_scale(sws_context_, frame_->data, frame_->linesize, 0, height,
						  rgb_frame_->data, rgb_frame_->linesize);
				rgb_frame_->width = width;
				rgb_frame_->height = height;

				const auto image_raw = cv::Mat(height, width, CV_8UC3, out_buffer);

				// 框选目标
				if (is_ready_to_select_target_) {
					if (is_ready_to_track_target_)
						target_tracker_->ClearTarget();
					target_tracker_->SelectTarget(image_raw, roi_);
					is_ready_to_select_target_ = false;
					is_ready_to_track_target_ = true;
				}

				// 跟踪
				if (is_ready_to_track_target_)
				{
					target_tracker_->TrackTarget(image_raw);

					// 绘制参考系
					rectangle(image_raw, cv::Rect2d(aim_rec_x_ - 10, aim_rec_y_ - 10, aim_rec_width_ + 20, aim_rec_height_ + 20), cv::Scalar(255, 0, 0), 2);
					if (aim_rec_width_ > 20 && aim_rec_height_ > 20)
						rectangle(image_raw, cv::Rect2d(aim_rec_x_ + 10, aim_rec_y_ + 10, aim_rec_width_ - 20, aim_rec_height_ - 20), cv::Scalar(255, 0, 0), 2);

					//line(image_raw, cv::Point(335, 235), cv::Point(335, 385), cv::Scalar(0, 0, 255), 2);
					//line(image_raw, cv::Point(375, 235), cv::Point(375, 385), cv::Scalar(0, 0, 255), 2);
					//line(image_raw, cv::Point(285, 275), cv::Point(425, 275), cv::Scalar(0, 0, 255), 2);
					//line(image_raw, cv::Point(285, 335), cv::Point(425, 335), cv::Scalar(0, 0, 255), 2);
				}
					

				//line(image_raw, cv::Point(584, 549), cv::Point(584, 699), cv::Scalar(0, 0, 255), 2);
				//line(image_raw, cv::Point(534, 549), cv::Point(534, 699), cv::Scalar(0, 0, 255), 2);
				//line(image_raw, cv::Point(484, 599), cv::Point(634, 599), cv::Scalar(0, 0, 255), 2);
				//line(image_raw, cv::Point(484, 649), cv::Point(634, 649), cv::Scalar(0, 0, 255), 2);

				// 从RGB数据中生成一张QImage图像
				auto image_temp = QImage(static_cast<const uchar*>(image_raw.data), width, height, QImage::Format_RGB888);

				// 深拷贝复制一份
				const auto image_out = image_temp.copy();
				
				// 释放Buffer	
				av_free(out_buffer);

				emit frame_ready_signal(image_out);
			}
		}
		length -= num_consumed;
		data_in += num_consumed;
	}

	data_in = data_in_ptr;
	delete[] data_in;
	av_packet_unref(packet_);
}

void FrameProcessor::ReadyToSelectTarget(const cv::Rect2d roi) {
	is_ready_to_select_target_ = true;
	roi_ = roi;
	//初始化目标框
	aim_rec_width_ = roi_.width;
	aim_rec_height_ = roi_.height;
	aim_rec_x_ = (960 - aim_rec_width_) / 2;
	aim_rec_y_ = (720 - aim_rec_height_) / 2;
}