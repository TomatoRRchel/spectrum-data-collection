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
#include "idefine_fun.h"
// #include "w_thread.h"

#include <errno.h>
using namespace std;
#define FILESIZE 2048

timespec get_current_time()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

long time_diff_ms(const timespec &start, const timespec &end)
{
    long sec_diff = end.tv_sec - start.tv_sec;
    long nsec_diff = end.tv_nsec - start.tv_nsec;
    return sec_diff * 1000 + nsec_diff / 1000000;
}
file_Manager ::file_Manager()
{
    cout << "file init" << endl;
    files.push_back({"./data0.txt", WAIT});
    files.push_back({"./data1.txt", WAIT});
#if NUM == 3
    files.push_back({"./data2.txt", FileState::WAIT});
#endif
}

void file_Manager::look()
{
    for (auto file : files)
    {
        printf("%s state %d\n", file.filename.c_str(), file.state);
    }
}

fileInfo *file_Manager::get_file_w()
{
    timespec start_time = get_current_time();

    while (1)
    {
        // printf("1\n");
        for (auto &file : files)
        {

            if (file.state == WAIT)
            {
#ifdef DE_BUG
                // printf("Found WAIT file: %s\n", file.filename.c_str());
#endif
                return &file;
            }
        }

        timespec current_time = get_current_time();
        long elapsed_ms = time_diff_ms(start_time, current_time);
        if (elapsed_ms >= TIME_OUT)
        {
            printf(" W Timeout\n");
            // files[0].state = OUT;
            return &err;
        }

        usleep(1000);
        // printf("search\n");
    }
    // throw runtime_error("No available WAIT file");
}
fileInfo *file_Manager::get_file_r()
{
    //    file_Manager::look();
    timespec start_time = get_current_time();
    while (1)
    {
        for (auto &file : files)
        {
            if (file.state == READY)
            {
                return &file;
            }
        }

        timespec current_time = get_current_time();
        long elapsed_ms = time_diff_ms(start_time, current_time);
        if (elapsed_ms >= TIME_OUT)
        {
            printf("up Timeout\n");
            return &err;
        }

        usleep(1000);
    }
    // throw runtime_error("No available READY file");
}

/*
file_Manager::file_Manager()
{
    files.push_back({"./data0.txt", FileState::WAIT});
    files.push_back({"./data1.txt", FileState::WAIT});
#ifdef NUM 3
    files.push_back({"./data2.txt", FileState::WAIT});
#endif
}

fileInfo &file_Manager::get_file_w()
{
    for (auto &file : files)
    {
        if (file.state == FileState::WAIT)
        {
            return file;
        }
    }
}

fileInfo &file_Manager::get_file_r()
{
    for (auto &file : files)
    {
        if (file.state == FileState::READY)
        {
            return file;
        }
    }
}
*/

// #include <iostream>
// #include <vector>
// #include <stdexcept>
// #include <mutex>
// #include <condition_variable>

// bool initializeOnce()
// {
//     if (libssh2_init(0) != 0)
//     {
//         cerr << "unable to initialize libssh2" << endl;
//         return false;
//     }
//     return true;
// }

// bool connectToServer(SSHConnectionState &state, const SSHConnectionInfo &connInfo)
// {
//     state.sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (state.sock < 0)
//     {
//         perror("create socket error");
//         return false;
//     }

//     struct hostent *host = gethostbyname(connInfo.host.c_str());
//     if (!host)
//     {
//         cerr << "host error: " << connInfo.host << endl;
//         close(state.sock);
//         return false;
//     }

//     sockaddr_in sin;
//     memset(&sin, 0, sizeof(sin));
//     sin.sin_family = AF_INET;
//     sin.sin_port = htons(connInfo.port);
//     sin.sin_addr = *((struct in_addr *)host->h_addr);

//     if (connect(state.sock, (struct sockaddr *)&sin, sizeof(sin)) != 0)
//     {
//         perror("connect error");
//         close(state.sock);
//         return false;
//     }

//     state.session = libssh2_session_init();
//     if (!state.session)
//     {
//         perror("ssh session error:");
//         close(state.sock);
//         return false;
//     }

//     libssh2_session_set_timeout(state.session, 10000);

//     int rc = libssh2_session_handshake(state.session, state.sock);
//     if (rc != 0)
//     {
//         perror("shake hand error:");
//         libssh2_session_free(state.session);
//         close(state.sock);
//         return false;
//     }

//     rc = libssh2_userauth_password(state.session, connInfo.username.c_str(), connInfo.password.c_str());
//     if (rc != 0)
//     {
//         perror("login error: ");
//         // << libssh2_session_last_error(state.session, nullptr, nullptr, 0) << endl;
//         libssh2_session_free(state.session);
//         close(state.sock);
//         return false;
//     }

//     state.sftp_session = libssh2_sftp_init(state.session);
//     if (!state.sftp_session)
//     {
//         cerr << "sftp error" << endl;
//         libssh2_session_free(state.session);
//         close(state.sock);
//         return false;
//     }
//     cout << "ssh services is ok" << endl;

//     return true;
// }

// bool Up_Class::uploadFile(SSHConnectionState &state, const string &localPath, const string &remotePath, unsigned long offset)
// {
//     LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(
//         state.sftp_session,
//         remotePath.c_str(),
//         LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL,
//         // | LIBSSH2_FXF_TRUNC,
//         LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR);

//     if (!sftp_handle)
//     {
//         perror("remotefile open error:");
//         return false;
//     }
//     FILE *file = fopen(localPath.c_str(), "r");
//     if (!file)
//     {
//         cerr << "localfile open error: " << localPath << endl;
//         libssh2_sftp_close(sftp_handle);
//         return false;
//     }

//     fseek(file, 0, SEEK_END);
//     long fileSize = ftell(file);
//     fseek(file, offset, SEEK_SET);
// #ifdef DE_BUG
//     cout << "ssh:read local: " << localPath << " (" << fileSize << ")" << endl;
// #endif
//     char buffer[4096 * 2];
//     size_t totalSent = 0;
//     size_t bytesRead;

//     while ((bytesRead = fread(buffer, 1, FILESIZE, file)) > 0)
//     {
//         size_t bytesSent = 0;
//         while (bytesSent < bytesRead)
//         {
//             int rc = libssh2_sftp_write(sftp_handle, buffer + bytesSent, bytesRead - bytesSent);
//             if (rc < 0)
//             {
//                 cerr << "ssh write error: " << rc << endl;
//                 fclose(file);
//                 libssh2_sftp_close(sftp_handle);
//                 return false;
//             }
//             bytesSent += rc;
//             totalSent += rc;
//         }
//     }

//     if (ferror(file))
//     {
//         cerr << "ssh:local file read error" << endl;
//         fclose(file);
//         libssh2_sftp_close(sftp_handle);
//         return false;
//     }
// #ifdef DE_BUG
//     cout << "ssh:file upload bytes: " << totalSent << endl;
// #endif
//     fclose(file);
//     libssh2_sftp_close(sftp_handle);
//     return true;
// }

// void disconnectSSH(SSHConnectionState &state)
// {
//     if (state.sftp_session)
//     {
//         libssh2_sftp_shutdown(state.sftp_session);
//     }
//     if (state.session)
//     {
//         libssh2_session_free(state.session);
//     }
//     if (state.sock >= 0)
//     {
//         close(state.sock);
//     }
// }

// void cleanupGlobal()
// {
//     libssh2_exit();
// }

// Up_Class::Up_Class(string _remoteDir)
//     : remoteDir(_remoteDir), channel(nullptr)
// {
//     connInfo.host = "127.0.0.1";
//     connInfo.port = 8080;
//     connInfo.username = "root";
//     connInfo.password = "074678";
//     connection.sock = -1;
//     connection.session = nullptr;
//     connection.sftp_session = nullptr;

//     if (ssh_init() != 0)
//     {
//         cout << "ssh_init failed" << endl;
//     }
// }

// Up_Class::~Up_Class()
// {
//     if (channel)
//     {
//         libssh2_channel_close(channel);
//         libssh2_channel_free(channel);
//         channel = nullptr;
//     }

//     // 只在连接有效时清理
//     if (connection.session || connection.sock >= 0)
//     {
//         disconnectSSH(connection);
//     }
// }

// int Up_Class::ssh_init()
// {
//     if (!initializeOnce())
//     {
//         cerr << "global error!" << endl;
//         return -1;
//     }

//     if (!connectToServer(connection, connInfo))
//     {
//         cerr << "connect failed!" << endl;
//         cleanupGlobal();
//         return -1;
//     }

//     return 0;
// }

// int Up_Class::ssh_upload(string &localFile, bool mode)
// {
//     if (mode)
//         localFile[7] += 2;
//     size_t pos = localFile.find_last_of("/");
//     string filename = (pos != string::npos) ? localFile.substr(pos + 1) : localFile;
//     string remotePath = remoteDir;

//     if (remoteDir.back() != '/')
//         remotePath += '/';
//     remotePath += filename;
// #ifdef DE_BUG
//     cout << "upload to" << remotePath << endl;
// #endif
//     if (!uploadFile(connection, localFile, remotePath, mode ? FILESIZE : 0))
//     {
//         cerr << "upload error: " << localFile << endl;
//         return 1;
//     }
//     return 0;
// }

// int Up_Class::ssh_command(string &command)
// {
//     channel = libssh2_channel_open_session(connection.session);
//     if (!channel)
//     {
//         cerr << "unable to open channel" << endl;
//         return -1;
//     }

//     int rc = libssh2_channel_exec(channel, command.c_str());
//     if (rc != 0)
//     {
//         char *err_msg;
//         int err_code;
//         libssh2_session_last_error(connection.session, &err_msg, &err_code, 0);
//         cerr << "command: " << command
//              << " error: " << err_msg
//              << " (code: " << err_code << ")" << endl;
//         libssh2_channel_free(channel);
//         channel = nullptr;
//         return -1;
//     }

//     char buffer[1024];
//     int bytesRead;
//     do
//     {
//         bytesRead = libssh2_channel_read(channel, buffer, sizeof(buffer) - 1);
//         if (bytesRead > 0)
//         {
//             buffer[bytesRead] = '\0';
//             cout << buffer;
//         }
//         else if (bytesRead == LIBSSH2_ERROR_EAGAIN)
//         {
//             usleep(100000);
//         }
//     } while (bytesRead > 0 || bytesRead == LIBSSH2_ERROR_EAGAIN);

//     int exitStatus = libssh2_channel_get_exit_status(channel);
//     libssh2_channel_close(channel);
//     libssh2_channel_free(channel);
//     channel = nullptr;

//     return bytesRead;
// }

// bool Up_Class::file_match(const string &filename, string &dir)
// {
//     string command = "test -f " + dir + filename + " && echo exist";
//     int result = ssh_command(command);
//     return (result > 0);
// }

// int Up_Class::start_shell()
// {
//     if (shellActive)
//         return 0;

//     if (channel)
//     {
//         libssh2_channel_close(channel);
//         libssh2_channel_free(channel);
//         channel = nullptr;
//     }

//     channel = libssh2_channel_open_session(connection.session);
//     if (!channel)
//     {
//         cerr << "unable to open channel" << endl;
//         return -1;
//     }

//     if (libssh2_channel_shell(channel) != 0)
//     {
//         cerr << "shell failed" << endl;
//         return -1;
//     }

//     shellActive = true;

//     return 0;
// }

// int Up_Class::write_to_shell(const string &command)
// {
//     if (!shellActive)
//         return -1;

//     string fullCommand = command + "\n";
//     int rc = libssh2_channel_write(channel, fullCommand.c_str(), fullCommand.size());
//     if (rc < 0)
//     {
//         cerr << "unable to write shell" << endl;
//         return -1;
//     }
//     return rc;
// }

// string Up_Class::read_from_shell()
// {
//     if (!shellActive)
//         return "";

//     string output;
//     char buffer[1024];
//     int bytesRead;

//     do
//     {
//         bytesRead = libssh2_channel_read(channel, buffer, sizeof(buffer) - 1);
//         if (bytesRead > 0)
//         {
//             buffer[bytesRead] = '\0';
//             output.append(buffer);
//         }
//         else if (bytesRead == LIBSSH2_ERROR_EAGAIN)
//         {
//             usleep(100000);
//         }
//     } while (bytesRead > 0 || bytesRead == LIBSSH2_ERROR_EAGAIN);

//     return output;
// }

// void Up_Class::close_shell()
// {
//     if (shellActive)
//     {
//         libssh2_channel_send_eof(channel);
//         libssh2_channel_close(channel);
//         libssh2_channel_free(channel);
//         channel = nullptr;
//         shellActive = false;
//         cout << "shell close" << endl;
//     }
// }