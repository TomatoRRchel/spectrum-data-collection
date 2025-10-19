#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <queue>
//#include <curl/curl.h> // 用于HTTP上传

using namespace std;
using namespace std::chrono;

// 文件状态枚举
enum class FileState {
    WRITING,    // 正在写入
    READY,      // 数据就绪待处理
    UPLOADED    // 已上传
};

// 文件信息结构
struct FileInfo {
    string filename;
    FileState state;
    size_t dataSize;
    system_clock::time_point creationTime;
};

// 文件状态管理器
class FileStateManager {
public:
    FileStateManager() {
        // 初始化三个文件状态
        files.push_back({ "data0.bin", FileState::UPLOADED, 0, system_clock::now() });
        files.push_back({ "data1.bin", FileState::UPLOADED, 0, system_clock::now() });
        files.push_back({ "data2.bin", FileState::UPLOADED, 0, system_clock::now() });

        // 设置第一个文件为写入状态
        files[0].state = FileState::WRITING;
        files[0].dataSize = 0;
    }

    // 获取当前写入文件
    FileInfo& getWritingFile() {
        unique_lock<mutex> lock(mtx);
        for (auto& file : files) {
            if (file.state == FileState::WRITING) {
                return file;
            }
        }

        // 如果没有WRITING状态文件，等待
        cv.wait(lock, [this] {
            for (auto& file : files) {
                if (file.state == FileState::WRITING) return true;
            }
            return false;
            });

        return getWritingFile(); // 递归调用确保返回
    }

    // 文件写满后切换状态
    void fileFull(const string& filename) {
        lock_guard<mutex> lock(mtx);
        for (auto& file : files) {
            if (file.filename == filename) {
                file.state = FileState::READY;
                file.creationTime = system_clock::now();

                // 寻找下一个可用的文件设置为WRITING
                for (auto& nextFile : files) {
                    if (nextFile.state == FileState::UPLOADED) {
                        nextFile.state = FileState::WRITING;
                        nextFile.dataSize = 0;
                        break;
                    }
                }
                cv.notify_all(); // 通知其他线程
                return;
            }
        }
    }

    // 获取待处理文件
    FileInfo getReadyFile() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] {
            for (auto& file : files) {
                if (file.state == FileState::READY) return true;
            }
            return false;
            });

        for (auto& file : files) {
            if (file.state == FileState::READY) {
                return file;
            }
        }
        return {}; // 不会执行到这里
    }

    // 标记文件已上传
    void markUploaded(const string& filename) {
        lock_guard<mutex> lock(mtx);
        for (auto& file : files) {
            if (file.filename == filename) {
                file.state = FileState::UPLOADED;
                return;
            }
        }
    }

private:
    vector<FileInfo> files;
    mutex mtx;
    condition_variable cv;
};

// 全局状态管理器
FileStateManager fileManager;
atomic<bool> running(true);

// 数据采集线程
void dataCollectionThread(vector<double>& Frequency, vector<float>& PowerSpec_dBm) {
    // 模拟设备信息结构
    struct TraceInfoType {
        int FullsweepTracePoints = 1024;
        int TotalHops = 8;
        int PartialsweepTracePoints = 128;
    } TraceInfo;

    // 模拟设备状态
    int Status = 0;
    int HopIndex = 0;
    int FrameIndex = 0;

    while (running) {
        auto& writingFile = fileManager.getWritingFile();
        ofstream outFile(writingFile.filename, ios::binary | ios::app);

        if (!outFile) {
            cerr << "无法打开文件: " << writingFile.filename << endl;
            continue;
        }

        for (int i = 0; i < TraceInfo.TotalHops; i++) {
            // 模拟数据采集
            for (int j = 0; j < TraceInfo.PartialsweepTracePoints; j++) {
                Frequency[i * TraceInfo.PartialsweepTracePoints + j] =
                    (i * TraceInfo.PartialsweepTracePoints + j) * 1.0;
                PowerSpec_dBm[i * TraceInfo.PartialsweepTracePoints + j] =
                    rand() % 100 * 1.0f;
            }

            // 写入频率和功率数据
            outFile.write(reinterpret_cast<const char*>(
                Frequency.data() + i * TraceInfo.PartialsweepTracePoints),
                sizeof(double) * TraceInfo.PartialsweepTracePoints);

            outFile.write(reinterpret_cast<const char*>(
                PowerSpec_dBm.data() + i * TraceInfo.PartialsweepTracePoints),
                sizeof(float) * TraceInfo.PartialsweepTracePoints);

            // 更新文件大小
            writingFile.dataSize += sizeof(double) * TraceInfo.PartialsweepTracePoints +
                sizeof(float) * TraceInfo.PartialsweepTracePoints;

            // 检查文件是否写满（设置10MB上限）
            if (writingFile.dataSize > 10 * 1024 * 1024) {
                outFile.close();
                fileManager.fileFull(writingFile.filename);
                break; // 跳出内层循环，获取新文件
            }
        }

        // 模拟采集间隔
        this_thread::sleep_for(milliseconds(100));
    }
}

// 数据显示线程
void dataDisplayThread() {
    while (running) {
        auto readyFile = fileManager.getReadyFile();
        ifstream inFile(readyFile.filename, ios::binary);

        if (!inFile) {
            cerr << "无法打开显示文件: " << readyFile.filename << endl;
            continue;
        }

        // 读取并显示数据
        vector<double> freqBuffer(128); // 假设每次读取128个点
        vector<float> powerBuffer(128);

        while (inFile) {
            inFile.read(reinterpret_cast<char*>(freqBuffer.data()),
                sizeof(double) * freqBuffer.size());

            inFile.read(reinterpret_cast<char*>(powerBuffer.data()),
                sizeof(float) * powerBuffer.size());

            if (inFile.gcount() > 0) {
                // 这里应该调用实际的示波器显示函数
                // displayToOscilloscope(freqBuffer, powerBuffer);

                cout << "显示文件: " << readyFile.filename
                    << ", 数据点: " << freqBuffer.size() << endl;

                // 控制显示刷新率
                this_thread::sleep_for(milliseconds(50));
            }
        }

        inFile.close();

        // 显示完成后标记文件可上传
        fileManager.markUploaded(readyFile.filename);
    }
}

// 文件上传回调函数
size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    // 可以在这里处理上传响应
    return size * nmemb;
}

// 数据上传线程
void dataUploadThread() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        cerr << "无法初始化CURL" << endl;
        return;
    }

    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // 启用详细输出

    while (running) {
        auto readyFile = fileManager.getReadyFile();

        // 设置上传URL和文件
        curl_easy_setopt(curl, CURLOPT_URL, "http://yourserver.com/upload");
        curl_mime* mime = curl_mime_init(curl);
        curl_mimepart* part = curl_mime_addpart(mime);

        curl_mime_name(part, "file");
        curl_mime_filedata(part, readyFile.filename.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // 执行上传
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            cout << "上传成功: " << readyFile.filename << endl;
            fileManager.markUploaded(readyFile.filename);
        }
        else {
            cerr << "上传失败: " << curl_easy_strerror(res) << endl;
        }

        curl_mime_free(mime);

        // 避免频繁重试
        this_thread::sleep_for(seconds(1));
    }

    curl_easy_cleanup(curl);
}

int main() {
    // 初始化数据数组
    vector<double> Frequency(1024); // 假设1024个频率点
    vector<float> PowerSpec_dBm(1024); // 1024个功率点

    // 创建线程
    thread collectionThread(dataCollectionThread, ref(Frequency), ref(PowerSpec_dBm));
    thread displayThread(dataDisplayThread);
    thread uploadThread(dataUploadThread);

    // 主线程监控
    cout << "系统启动，按Enter键停止..." << endl;
    cin.get();

    // 停止线程
    running = false;

    // 通知所有等待的线程
    {
        lock_guard<mutex> lock(fileManager.mtx);
        fileManager.cv.notify_all();
    }

    // 等待线程结束
    collectionThread.join();
    displayThread.join();
    uploadThread.join();

    cout << "系统已安全停止" << endl;
    return 0;
}