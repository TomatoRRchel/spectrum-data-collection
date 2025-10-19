#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include <iterator>
#include <algorithm>
#include "ssh_function.h"

#define NUM 2
#define TIME_TEST

using namespace std;
enum class FileState {
	WAIT,
	UP,
	READY
};

struct fileInfo {
	string filename;
	FileState state;
};
class fileManager {
public:
	fileManager() {
		files.push_back({ "./data0.txt",FileState::WAIT });
		files.push_back({ "./data1.txt",FileState::WAIT });
#if NUM==3	files.push_back({ "./data2.txt",FileState::WAIT });
#endif
	}

	fileInfo& get_file_w() {
		for (auto& file : files) {
			if (file.state == FileState::WAIT)
			{
				return file;
			}
		}
		throw runtime_error("No available WAIT file");

	}
	fileInfo& get_file_r() {
		for (auto& file : files) {
			if (file.state == FileState::READY)
			{
				return file;
			}
		}
		throw runtime_error("No available READY file");

	}
private:
	vector<fileInfo> files;
};
vector < int > dat(256);

//ofstream file;
ofstream output_file("./1.txt");
fileManager file_Manager;
vector<mutex> mut_arr(NUM);
unsigned int ccount = 0;
void thread_write() {
	//dat.clear();
	while (1) {
		try {
#ifdef TIME_TEST
			auto start = chrono::high_resolution_clock::now();
#endif			
			fileInfo& file_temp = file_Manager.get_file_w();
			//cout << "find_file:" << file_temp.filename << "---write" << endl;

			switch (file_temp.filename[6])
			{
			case '0':mut_arr[0].lock(); break;
			case '1':mut_arr[1].lock(); break;
#if NUM==3

			case '2':mut_arr[2].lock(); break;
#endif
			default:
				break;
			}
			ofstream w_file(file_temp.filename);
			for (unsigned char i = 0; ; i++)
			{
				dat[i] = ccount++;
				if (i == 255)
				{
					break;
				}
			}
			ostream_iterator<int> output_iterator(w_file, "\n");
			copy(dat.begin(), dat.end(), output_iterator);
			output_file.close();

			switch (file_temp.filename[6])
			{
			case '0':mut_arr[0].unlock(); break;
			case '1':mut_arr[1].unlock(); break;
#if NUM==3
			case '2':mut_arr[2].unlock(); break;
#endif
			default:
				break;
			}
			cout << "********write over unlock*************" << endl;
			file_temp.state = FileState::READY;
			//clock_t end = clock();
#ifdef TIME_TEST
			auto end = chrono::high_resolution_clock::now();
			chrono::duration<double, std::milli> elapsed = end - start;
			cout << "W time used: " << elapsed.count() << " ms\n";
#endif
		}
		catch (const exception& e) {
			;//cerr << "Error in write: " << e.what() << endl;
		}
	}
}
vector < string > get_string;
void myp(string val) {
	cout << val << endl;
}
void read() {
	//get_string.clear();
	while (1) {
		try {
#ifdef TIME_TEST
			auto start = chrono::high_resolution_clock::now();
#endif
			fileInfo& file_temp = file_Manager.get_file_r();
			//cout << "find_file:" << file_temp.filename << "---read" << endl;
			switch (file_temp.filename[6])
			{
			case '0':mut_arr[0].lock(); break;
			case '1':mut_arr[1].lock(); break;
#if NUM==3			
			case '2':mut_arr[2].lock(); break;
#endif
			default:
				break;
			}
			get_string.clear();
			ifstream r_file(file_temp.filename);
			while (r_file) {
				string line;
				getline(r_file, line);
				if (!line.empty())
					get_string.push_back(line);
			}
			//cout << "last data:" <<get_string.back() << endl;
			r_file.close();

			switch (file_temp.filename[6])
			{
			case '0':mut_arr[0].unlock(); break;
			case '1':mut_arr[1].unlock(); break;
			case '2':mut_arr[2].unlock(); break;
			default:
				break;
			}
			//cout << "------------read last data:" << get_string.back() << "-------------" << endl;
			//for_each(get_string.begin(), get_string.end(), myp);
			file_temp.state = FileState::WAIT;
#ifdef TIME_TEST
			auto end = chrono::high_resolution_clock::now();
			chrono::duration<double, std::milli> elapsed = end - start;
			cout << "R time used: " << elapsed.count() << " ms\n";
#endif
		}
		catch (const exception& e) {
			;//cerr << "Error in read: " << e.what() << endl;
		}
	}
}
void up() {
	SSHConnectionInfo connInfo = {
		"127.0.0.1",    // 本地地址
		8080,          // 本地转发端口
		"root",// 目标服务器用户名
		"074678" // 目标服务器密码
	};
	// 远程目录
	SSHConnectionState connection;
	if (global_init(connection, connInfo)) {
		cout << "init failed" << endl;
	}

	while (1) {
		try {
#ifdef TIME_TEST
			auto start = chrono::high_resolution_clock::now();
#endif
			fileInfo& file_temp = file_Manager.get_file_r();
			cout << "find_file:" << file_temp.filename << "---read" << endl;
			switch (file_temp.filename[6])
			{
			case '0':mut_arr[0].lock(); break;
			case '1':mut_arr[1].lock(); break;
#if NUM==3			
			case '2':mut_arr[2].lock(); break;
#endif
			default:
				break;
			}
			string localFile = file_temp.filename;
			upload_file(connection, localFile);




			switch (file_temp.filename[6])
			{
			case '0':mut_arr[0].unlock(); break;
			case '1':mut_arr[1].unlock(); break;
#ifdef TIME_TEST
			case '2':mut_arr[2].unlock(); break;
#endif
			default:
				break;
			}
			//cout << "------------read last data:" << get_string.back() << "-------------" << endl;
			//for_each(get_string.begin(), get_string.end(), myp);
			file_temp.state = FileState::WAIT;
#ifdef TIME_TEST
			auto end = chrono::high_resolution_clock::now();
			chrono::duration<double, std::milli> elapsed = end - start;
			cout << "R time used: " << elapsed.count() << " ms\n";
#endif

		}
		catch (const exception& e) {
			;//cerr << "Error in read: " << e.what() << endl;
		}
	}
	disconnectSSH(connection);
	cleanupGlobal();
}
int Count = 0;
mutex mut;
void task_0(int par) {
	for (int i = 0; i < 10; i++) {
		mut.lock();
		Count++;
		cout << "now thread " << par << " :" << Count << endl;
		mut.unlock();
		//this_thread::sleep_for(chrono::milliseconds(10));
	//auto start = chrono::high_resolution_clock::now();
	//this_thread::sleep_for(chrono::milliseconds(100));
	//auto end = chrono::high_resolution_clock::now();
	//auto Ti = end - start;
	//cout << "sleep:" << Ti.count() << endl;
	}
	this_thread::sleep_for(chrono::milliseconds(10));
}
void task_1() {
	for (int i = 0; i < 10; i++) {
		mut.lock();
		Count++;

		cout << "now thread 1:" << Count << endl;
		mut.unlock();
		//this_thread::sleep_for(chrono::milliseconds(100));
	}
}
int main() {

	thread t1(thread_write);
	thread t2(up);
	t1.join();
	t2.join();
	//write();
	////_sleep(1);
	//write();
	//read();

	//read();
	//write();
	//read();
	//write();
	//read();
	//thread task0(task_0,0);
	//thread task1(task_0,1);

	//	task0.join();
	//	task1.join();


	return 0;
}
