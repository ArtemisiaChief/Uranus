#include "uranus.hpp"

Uranus::Uranus(QWidget *parent) : QMainWindow(parent) {

	// 控制命令收发线程
	drone_control_ = DroneControl::GetInstance();
	drone_control_->moveToThread(&drone_control_thread_);
	connect(this, &Uranus::connect_signal, drone_control_, &DroneControl::Connect);
	connect(this, &Uranus::takeoff_signal, drone_control_, &DroneControl::Takeoff);
	connect(this, &Uranus::land_signal, drone_control_, &DroneControl::Land);
	connect(this, &Uranus::stream_open_signal, drone_control_, &DroneControl::OpenStream);
	connect(this, &Uranus::stream_close_signal, drone_control_, &DroneControl::CloseStream);
	connect(this, &Uranus::rc_signal, drone_control_, &DroneControl::SetRC);
	connect(this, &Uranus::flip_signal, drone_control_, &DroneControl::Flip);
	connect(this, &Uranus::speed_change_signal, drone_control_, &DroneControl::SetSpeed);
	connect(drone_control_, &DroneControl::stream_closed_signal, this, &Uranus::set_frame_to_black);
	drone_control_thread_.start();

	// 状态接收更新线程
	drone_status_ = DroneStatus::GetInstance();
	drone_status_->moveToThread(&drone_status_thread_);
	connect(drone_status_,&DroneStatus::update_states_signal,this, &Uranus::show_status);
	drone_status_thread_.start();

	// 视频流接收线程
	drone_stream_ = DroneStream::GetInstance();
	drone_stream_->moveToThread(&drone_stream_thread_);
	drone_stream_thread_.start();

	// 帧处理线程
	frame_processor_ = FrameProcessor::GetInstance();
	frame_processor_->moveToThread(&frame_processor_thread_);
	connect(frame_processor_, &FrameProcessor::frame_ready_signal, this, &Uranus::show_frame);
	connect(drone_stream_, &DroneStream::construct_frame_signal, frame_processor_, &FrameProcessor::ConsturctFrame);
	frame_processor_thread_.start();

	// GUI
	ui.setupUi(this);
	ui.frame_label->setScaledContents(true);

	// 设置初始视频帧为纯黑单色图
	set_frame_to_black();

	// 能获取键盘焦点
	this->grabKeyboard();
}

Uranus::~Uranus() {
	emit stream_close_signal();

	drone_control_->deleteLater();
	drone_control_thread_.quit();
	drone_control_thread_.wait();

	drone_status_->deleteLater();
	drone_status_thread_.quit();
	drone_status_thread_.wait();

	drone_stream_->deleteLater();
	drone_stream_thread_.quit();
	drone_stream_thread_.wait();

	frame_processor_->deleteLater();
	frame_processor_thread_.quit();
	frame_processor_thread_.wait();
}

void Uranus::keyPressEvent(QKeyEvent* key_event) {
	if (!drone_control_->get_is_connected())
		return;

	const auto key_value = key_event->key();
	switch (key_value) {
	case Qt::Key_W:
		rc_[1] = stick_;
		break;
	case Qt::Key_A:
		rc_[0] = -stick_;
		break;
	case Qt::Key_S:
		rc_[1] = -stick_;
		break;
	case Qt::Key_D:
		rc_[0] = stick_;
		break;
	case Qt::Key_Q:
		rc_[3] = -stick_;
		break;
	case Qt::Key_E:
		rc_[3] = stick_;
		break;
	case Qt::Key_Space:
		rc_[2] = stick_;
		break;
	case Qt::Key_Shift:
		rc_[2] = -stick_;
		break;
	case Qt::Key_Up:
		emit flip_signal('f');
		break;
	case Qt::Key_Left:
		emit flip_signal('l');
		break;
	case Qt::Key_Right:
		emit flip_signal('r');
		break;
	case Qt::Key_Down:
		emit flip_signal('b');
		break;
	case Qt::Key_CapsLock:
		if (drone_control_->get_is_takeoff())
			emit land_signal();
		else
			emit takeoff_signal();
		break;
	case Qt::Key_R:
		if (drone_control_->get_is_streaming())
			emit stream_close_signal();
		else
			emit stream_open_signal();
		break;
	default:
		break;
	}

	emit rc_signal(rc_[0], rc_[1], rc_[2], rc_[3]);
}

void Uranus::keyReleaseEvent(QKeyEvent* key_event) {
	if (!drone_control_->get_is_connected())
		return;

	const auto key_value = key_event->key();
	switch (key_value) {
	case Qt::Key_W:
		rc_[1] = 0;
		break;
	case Qt::Key_A:
		rc_[0] = 0;
		break;
	case Qt::Key_S:
		rc_[1] = 0;
		break;
	case Qt::Key_D:
		rc_[0] = 0;
		break;
	case Qt::Key_Q:
		rc_[3] = 0;
		break;
	case Qt::Key_E:
		rc_[3] = 0;
		break;
	case Qt::Key_Space:
		rc_[2] = 0;
		break;
	case Qt::Key_Shift:
		rc_[2] = 0;
		break;
	default:
		break;
	}

	emit rc_signal(rc_[0], rc_[1], rc_[2], rc_[3]);
}

void Uranus::on_connect_btn_clicked() {
	emit connect_signal();
}

void Uranus::on_connect_btn_pressed() const {
	ui.connect_btn->setStyleSheet("background-color:red; border:10px solid; border-color: rgb(85, 255, 127, 50%); border-radius:50%;");
}

void Uranus::on_connect_btn_released() const {
	ui.connect_btn->setStyleSheet("background-color: rgb(255, 85, 0); border:10px solid; border-color: rgb(85, 255, 127, 50%); border-radius:50%;");
}

void Uranus::on_rc_factor_slider_valueChanged(const int value) {
	stick_ = value;
}

void Uranus::on_speed_slider_valueChanged(const int value) {
	emit speed_change_signal(value);
}

void Uranus::show_frame(const QImage image) const {
	ui.frame_label->setPixmap(QPixmap::fromImage(image));
}

void Uranus::show_status(int* params) const {
	// 计算合速度
	const auto recent_speed = sqrt(params[3] * params[3] + params[4] * params[4] + params[5] * params[5]);

	// 更新合速度
	ui.speed_label->setText(QString("Speed: %1 cm/s").arg(QString::number(recent_speed, 'f', 2)));

	// 更新俯仰角度、横滚角度、偏航角度
	ui.pitch_label->setText(QString("pitch: %1 ").arg(params[0]));
	ui.roll_label->setText(QString("roll: %1 ").arg(params[1]));
	ui.yaw_label->setText(QString("yaw: %1 ").arg(params[2]));

	// 更新x、y、z轴速度
	ui.vgx_label->setText(QString("vgx: %1 cm/s").arg(params[3]));
	ui.vgy_label->setText(QString("vgy: %1 cm/s").arg(params[4]));
	ui.vgz_label->setText(QString("vgz: %1 cm/s").arg(params[5]));

	// 更新主板最低/高温度
	ui.templ_label->setText(QString("templ: %1").arg(params[6]));
	ui.temph_label->setText(QString("temph: %1 ").arg(params[7]));

	// ToF距离、相对起飞点高度、当前电量、气压计测量高度、电机运转时间
	ui.tof_label->setText(QString("tof: %1 cm").arg(params[8]));
	ui.h_label->setText(QString("h: %1 cm").arg(params[9]));
	ui.battery_bar->setValue(params[10]);
	ui.baro_label->setText(QString("baro: %1 cm").arg(params[11]));
	ui.time_label->setText(QString("time: %1 s").arg(params[12]));

	// 更新x、y、z轴加速度
	ui.agx_label->setText(QString("agx: %1 mg").arg(params[13]));
	ui.agy_label->setText(QString("agy: %1 mg").arg(params[14]));
	ui.agz_label->setText(QString("agz: %1 mg").arg(params[15]));
}

void Uranus::set_frame_to_black() const {
	const auto black_image = cv::Mat(1280, 720, CV_8UC1, cv::Scalar(0));
	const QImage image_rgb(static_cast<const uchar *>(black_image.data), black_image.cols, black_image.rows, black_image.step, QImage::Format_Grayscale8);
	ui.frame_label->setPixmap(QPixmap::fromImage(image_rgb));
}


