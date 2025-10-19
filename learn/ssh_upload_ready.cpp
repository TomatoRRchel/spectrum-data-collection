#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssh2.lib")

using namespace std;

// SSH连接信息结构
struct SSHConnectionInfo {
	string host;         // 服务器地址
	int port = 22;       // SSH端口
	string username;     // 用户名
	string password;     // 密码
};

// SSH连接状态结构
struct SSHConnectionState {
	SOCKET sock;                // Socket连接
	LIBSSH2_SESSION* session;   // SSH会话
	LIBSSH2_SFTP* sftp_session; // SFTP会话
};

// ==== 一次性初始化 ====
bool initializeOnce() {
	// 1. 初始化Winsock - 整个程序只需要一次
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cerr << "WSAStartup失败: " << WSAGetLastError() << endl;
		return false;
	}

	// 2. 初始化libssh2库 - 整个程序只需要一次
	if (libssh2_init(0) != 0) {
		cerr << "无法初始化libssh2" << endl;
		WSACleanup();
		return false;
	}

	return true;
}

// ==== 连接初始化 ====
bool connectToServer(SSHConnectionState& state, const SSHConnectionInfo& connInfo) {
	// 1. 创建socket - 每个连接需要一次
	state.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (state.sock == INVALID_SOCKET) {
		cerr << "无法创建socket: " << WSAGetLastError() << endl;
		return false;
	}

	// 2. 设置服务器地址
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(connInfo.port);

	// 使用 getaddrinfo 替代 gethostbyname
	struct addrinfo hints, * res = nullptr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(connInfo.host.c_str(), nullptr, &hints, &res);
	if (status != 0) {
		cerr << "解析主机名失败: " << gai_strerror(status) << endl;
		closesocket(state.sock);
		return false;
	}

	// 复制地址信息
	sockaddr_in* addr = (sockaddr_in*)res->ai_addr;
	sin.sin_addr = addr->sin_addr;

	// 3. 连接到服务器
	if (connect(state.sock, (sockaddr*)&sin, sizeof(sin)) != 0) {
		cerr << "连接服务器失败: " << WSAGetLastError() << endl;
		freeaddrinfo(res); // 释放资源
		closesocket(state.sock);
		return false;
	}

	freeaddrinfo(res); // 释放资源

	cout << "成功连接到服务器: " << connInfo.host << ":" << connInfo.port << endl;

	// 4. 创建SSH会话
	state.session = libssh2_session_init();
	if (!state.session) {
		cerr << "无法创建SSH会话" << endl;
		closesocket(state.sock);
		return false;
	}

	// 5. 设置超时
	libssh2_session_set_timeout(state.session, 10000); // 10秒超时

	// 6. 开始SSH握手
	int rc = libssh2_session_handshake(state.session, state.sock);
	if (rc != 0) {
		cerr << "SSH握手失败: " << libssh2_session_last_error(state.session, nullptr, nullptr, 0) << endl;
		libssh2_session_free(state.session);
		closesocket(state.sock);
		return false;
	}

	cout << "SSH握手成功" << endl;

	// 7. 身份验证
	rc = libssh2_userauth_password(state.session, connInfo.username.c_str(), connInfo.password.c_str());
	if (rc != 0) {
		cerr << "身份验证失败: " << libssh2_session_last_error(state.session, nullptr, nullptr, 0) << endl;
		libssh2_session_free(state.session);
		closesocket(state.sock);
		return false;
	}

	cout << "身份验证成功!" << endl;

	// 8. 初始化SFTP会话
	state.sftp_session = libssh2_sftp_init(state.session);
	if (!state.sftp_session) {
		cerr << "无法初始化SFTP会话" << endl;
		libssh2_session_free(state.session);
		closesocket(state.sock);
		return false;
	}

	cout << "SFTP会话已初始化" << endl;

	return true;
}

// ==== 每次上传需要的初始化 ====
bool uploadFile(SSHConnectionState& state, const string& localPath, const string& remotePath) {
	// 1. 打开远程文件 - 每次上传都需要
	LIBSSH2_SFTP_HANDLE* sftp_handle = libssh2_sftp_open(
		state.sftp_session,
		remotePath.c_str(),
		LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
		LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR
	);

	if (!sftp_handle) {
		cerr << "无法打开远程文件: " << remotePath << endl;
		return false;
	}

	// 2. 打开本地文件 - 每次上传都需要
	//ifstream file(localPath, ios::binary | ios::ate);
	ifstream file(localPath, ios::ate);
	if (!file.is_open()) {
		cerr << "无法打开本地文件: " << localPath << endl;
		libssh2_sftp_close(sftp_handle);
		return false;
	}

	// 3. 获取文件大小 - 每次上传都需要
	streamsize fileSize = file.tellg();
	file.seekg(0, ios::beg);

	cout << "开始上传文件: " << localPath << " (" << fileSize << " 字节)" << endl;

	// 4. 读取并上传文件内容 - 每次上传都需要
	vector<char> buffer(4096); // 4KB缓冲区
	streamsize totalSent = 0;

	while (!file.eof()) {
		file.read(buffer.data(), buffer.size());
		streamsize bytesRead = file.gcount();

		if (bytesRead > 0) {
			// 写入远程文件
			ssize_t bytesSent = 0;
			while (bytesSent < bytesRead) {
				int rc = libssh2_sftp_write(sftp_handle, buffer.data() + bytesSent, bytesRead - bytesSent);
				if (rc < 0) {
					cerr << "文件写入失败: " << rc << endl;
					file.close();
					libssh2_sftp_close(sftp_handle);
					return false;
				}
				bytesSent += rc;
				totalSent += rc;

				// 显示进度
				//double progress = static_cast<double>(totalSent) / fileSize * 100;
				//cout << "\r上传进度: " << fixed << setprecision(1) << progress << "%" << flush;
			}
		}
	}

	cout << endl << "文件上传完成" << endl;

	// 5. 关闭文件句柄 - 每次上传都需要
	libssh2_sftp_close(sftp_handle);
	file.close();

	return true;
}

// ==== 清理连接 ====
void disconnectSSH(SSHConnectionState& state) {
	// 关闭SFTP会话
	if (state.sftp_session) {
		libssh2_sftp_shutdown(state.sftp_session);
	}

	// 关闭SSH会话
	if (state.session) {
		libssh2_session_free(state.session);
	}

	// 关闭socket
	if (state.sock != INVALID_SOCKET) {
		closesocket(state.sock);
	}
}

// ==== 程序结束时的全局清理 ====
void cleanupGlobal() {
	// 清理libssh2
	libssh2_exit();

	// 清理Winsock
	WSACleanup();
}

int main() {
	// 配置连接信息
	SSHConnectionInfo connInfo = {
		"127.0.0.1",    // 本地地址
		8080,          // 本地转发端口
		"root",// 目标服务器用户名
		"074678" // 目标服务器密码
	};

	// 本地文件列表
	vector<string> localFiles = {
		"./1.txt",  // 第一个文件
		"./2.txt"   // 第二个文件
	};

	// 远程目录
	string remoteDir = ".";

	// ==== 一次性初始化 ====
	if (!initializeOnce()) {
		cerr << "全局初始化失败!" << endl;
		return 1;
	}

	// ==== 连接初始化 ====
	SSHConnectionState connection;
	if (!connectToServer(connection, connInfo)) {
		cerr << "连接服务器失败!" << endl;
		cleanupGlobal();
		return 1;
	}

	// ==== 每次上传需要的初始化 ====
	for (const auto& localFile : localFiles) {
		// 提取文件名
		size_t pos = localFile.find_last_of("\\/");
		string filename = (pos != string::npos) ? localFile.substr(pos + 1) : localFile;

		// 构建远程路径
		string remotePath = remoteDir;
		if (remoteDir.back() != '/') remotePath += '/';
		remotePath += filename;

		// 上传文件
		if (!uploadFile(connection, localFile, remotePath)) {
			cerr << "文件上传失败: " << localFile << endl;
		}
	}

	// ==== 清理连接 ====
	disconnectSSH(connection);

	// ==== 程序结束时的全局清理 ====
	cleanupGlobal();

	cout << "所有操作完成!" << endl;
	return 0;
}