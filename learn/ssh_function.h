#ifndef SSH_FUNCTION_H_
#define SSH_FUNCTION_H_

#include <libssh2.h>
#include <libssh2_sftp.h>
#include <string>
using namespace std;
struct SSHConnectionInfo {
	string host;         // 服务器地址
	int port = 22;       // SSH端口
	string username;     // 用户名
	string password;     // 密码
};
struct SSHConnectionState {
	SOCKET sock;                // Socket连接
	LIBSSH2_SESSION* session;   // SSH会话
	LIBSSH2_SFTP* sftp_session; // SFTP会话
};
void disconnectSSH(SSHConnectionState& state);
void cleanupGlobal();

int global_init(SSHConnectionState& connection, SSHConnectionInfo& connInfo);

int upload_file(SSHConnectionState& connection, string& localFile);
#endif // ! SSH_FUNCTION_H

#pragma once
