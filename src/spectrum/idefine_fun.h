#ifndef _IDEFINE_H_
#define _IDEFINE_H_

#include <libssh2.h>
#include <iostream>
#include <vector>
#include <libssh2_sftp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
// #include "w_thread.h"
#define TIME_OUT 10000
using namespace std;

#define WAIT 0
#define READY 1
#define OUT -1
// file_Manager fileManager;
struct fileInfo
{
    string filename;
    char state;
};
class file_Manager
{
public:
    file_Manager();
    fileInfo *get_file_w();
    fileInfo *get_file_r();
    void look();

private:
    vector<fileInfo> files;
    fileInfo err = {"", OUT};
};
// enum FileState
// {
//     WAIT,
//     READY
// };
// struct fileInfo
// {
//     string filename;
//     FileState state;
// };
// class file_Manager
// {
// public:
//     file_Manager();
//     fileInfo &get_file_w();
//     fileInfo &get_file_r();

// private:
//     vector<fileInfo> files;
// };

struct SSHConnectionInfo
{
    string host;
    int port = 8080;
    string username;
    string password;
};

struct SSHConnectionState
{
    int sock;
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;
};

class Up_Class
{
private:
    SSHConnectionInfo connInfo;
    SSHConnectionState connection;
    LIBSSH2_CHANNEL *channel;
    string remoteDir;
    bool shellActive = false;

    int ssh_init();
    bool uploadFile(SSHConnectionState &state, const string &localPath, const string &remotePath, unsigned long offset);

public:
    Up_Class(string _remoteDir);
    ~Up_Class();
    int start_shell();
    string read_from_shell();
    void close_shell();
    int write_to_shell(const string &command);
    int ssh_upload(string &localFile, bool mode);
    int ssh_command(string &command);
    bool file_match(const string &filename, string &dir);
};

#endif
