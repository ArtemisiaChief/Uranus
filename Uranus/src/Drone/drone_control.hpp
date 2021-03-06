#pragma once
#include <QObject>
#include <QUdpSocket>

#define IP_DRONE "192.168.10.1"
#define REMOTE_PORT_CONTROL 8889

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    TypeName& operator=(const TypeName&)

class DroneControl : public QObject {

	Q_OBJECT

public:

	static DroneControl* GetInstance();

	bool get_is_connected() const;
	bool get_is_takeoff() const;
	bool get_is_streaming() const;

private:

	// 限制编译器自动生成的拷贝构造函数和赋值构造函数
	DISALLOW_COPY_AND_ASSIGN(DroneControl);

	// 构造函数
	DroneControl();

	// 单例
	static DroneControl* drone_control_;

	// 收发控制命令的UDP Socket
	QUdpSocket* socket_;

	// 返回值缓存区
	char* buffer_;

	// 状态布尔值
	bool is_connected_;
	bool is_takeoff_;
	bool is_streaming_;

public slots:

	// 连接
	void Connect();

	// 起飞、降落
	void Takeoff();
	void Land();

	// 向一个方向直线移动一定距离，distance范围20-500（cm）
	void MoveForward(const int distance) const;
	void MoveBackward(const int distance) const;
	void MoveLeft(const int distance) const;
	void MoveRight(int distance) const;
	void MoveUp(const int distance) const;
	void MoveDown(const int distance) const;

	// 旋转方向，degree范围1-3600（度）
	void RotateLeft(const int degree) const;
	void RotateRight(const int degree) const;

	// 翻滚，参数为字符，'l' - left，'r' - right，'f' - forward，'b' - back，
	void Flip(const char direction) const;

	// 设置杆量，参数分别为横滚、俯仰、油门、偏航，范围均为（-100，100）
	void SetRC(const int roll, const int pitch, const int throttle, const int yaw) const;

	// 设置飞行速度，value范围10-100（cm/s）
	void SetSpeed(const int value) const;

	// 打开、关闭视频流
	void OpenStream();
	void CloseStream();

	// 紧急制动
	void Emergency() const;
};