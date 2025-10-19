#include <iostream>
#include <string>
#include <libssh2.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // 链接Windows Socket库
#pragma comment(lib, "libssh2.lib") // 链接libssh2库

using namespace std;

// SSH连接信息结构体
struct SSHConnectionInfo {
	string host;         // 服务器地址
	int port = 22;       // SSH端口（默认22）
	string username;     // 用户名
	string password;     // 密码
};

// 初始化Windows Socket
bool initWinsock() {
	WSADATA wsaData;  // 存储Winsock初始化信息
	// 初始化Winsock 2.2版本
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		cerr << "WSAStartup失败: " << result << endl;
		return false;
	}
	return true;
}

// SSH登录服务器函数
bool sshLogin(const SSHConnectionInfo& connInfo) {
	// 1. 初始化Winsock
	if (!initWinsock()) {
		return false;
	}

	// 2. 初始化libssh2库
	if (libssh2_init(0) != 0) {
		cerr << "无法初始化libssh2" << endl;
		WSACleanup();  // 清理Winsock
		return false;
	}

	// 3. 创建socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		cerr << "无法创建socket: " << WSAGetLastError() << endl;
		libssh2_exit();  // 清理libssh2
		WSACleanup();    // 清理Winsock
		return false;
	}

	// 4. 设置服务器地址信息
	sockaddr_in sin;
	sin.sin_family = AF_INET;      // IPv4地址族
	sin.sin_port = htons(connInfo.port); // 端口号（转换为网络字节序）

	// 解析主机名或IP地址
	hostent* host = gethostbyname(connInfo.host.c_str());
	if (!host) {
		cerr << "无法解析主机名: " << connInfo.host << endl;
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}
	sin.sin_addr.s_addr = *reinterpret_cast<unsigned long*>(host->h_addr);

	// 5. 连接到服务器
	if (connect(sock, (sockaddr*)&sin, sizeof(sin)) != 0) {
		cerr << "连接服务器失败: " << WSAGetLastError() << endl;
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	cout << "成功连接到服务器: " << connInfo.host << ":" << connInfo.port << endl;

	// 6. 创建SSH会话
	LIBSSH2_SESSION* session = libssh2_session_init();
	if (!session) {
		cerr << "无法创建SSH会话" << endl;
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	// 7. 设置会话超时（10秒）
	libssh2_session_set_timeout(session, 10000);

	// 8. 开始SSH握手
	int rc = libssh2_session_handshake(session, sock);
	if (rc != 0) {
		cerr << "SSH握手失败: " << libssh2_session_last_error(session, nullptr, nullptr, 0) << endl;
		libssh2_session_free(session);
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	cout << "SSH握手成功" << endl;

	// 9. 获取支持的认证方法
	const char* authlist = libssh2_userauth_list(session, connInfo.username.c_str(), connInfo.username.size());
	if (!authlist) {
		cerr << "无法获取认证方法列表" << endl;
		libssh2_session_free(session);
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	cout << "支持的认证方法: " << authlist << endl;

	// 10. 使用密码认证
	rc = libssh2_userauth_password(session, connInfo.username.c_str(), connInfo.password.c_str());
	if (rc != 0) {
		cerr << "身份验证失败: " << libssh2_session_last_error(session, nullptr, nullptr, 0) << endl;
		libssh2_session_free(session);
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	cout << "身份验证成功!" << endl;

	// 11. 打开SSH通道
	LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(session);
	if (!channel) {
		cerr << "无法打开SSH通道" << endl;
		libssh2_session_free(session);
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	cout << "SSH通道已打开" << endl;

	// 12. 执行简单命令（获取服务器信息）
	rc = libssh2_channel_exec(channel, "uname -a");
	if (rc != 0) {
		cerr << "无法执行命令" << endl;
		libssh2_channel_free(channel);
		libssh2_session_free(session);
		closesocket(sock);
		libssh2_exit();
		WSACleanup();
		return false;
	}

	// 13. 读取命令输出
	char buffer[1024];
	int bytesRead;
	cout << "服务器信息: ";
	do {
		bytesRead = libssh2_channel_read(channel, buffer, sizeof(buffer));
		if (bytesRead > 0) {
			cout.write(buffer, bytesRead);
		}
	} while (bytesRead > 0);
	cout << endl;

	// 14. 关闭通道
	libssh2_channel_close(channel);
	libssh2_channel_free(channel);

	// 15. 清理资源
	libssh2_session_free(session);
	closesocket(sock);
	libssh2_exit();
	WSACleanup();

	cout << "SSH会话已关闭" << endl;
	return true;
}

int main() {
	// 配置连接信息
	SSHConnectionInfo connInfo = {
		"your.server.com",  // 服务器地址
		22,                 // SSH端口
		"your_username",    // 用户名
		"your_password"     // 密码
	};

	cout << "尝试登录服务器..." << endl;
	cout << "服务器: " << connInfo.host << ":" << connInfo.port << endl;
	cout << "用户名: " << connInfo.username << endl;

	if (sshLogin(connInfo)) {
		cout << "SSH登录成功!" << endl;
	}
	else {
		cerr << "SSH登录失败!" << endl;
	}

	return 0;
}