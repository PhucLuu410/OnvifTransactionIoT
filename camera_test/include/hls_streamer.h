#ifndef CAMERA_TEST_HLS_STREAMER_H
#define CAMERA_TEST_HLS_STREAMER_H

#include <string>

namespace Eyerus {

struct StreamConfig {
    std::string gstExecutable;          // Đường dẫn gst-launch-1.0 (để trống nếu đã có trong PATH)
    std::string outputDir = "hls";      // Thư mục lưu file .m3u8 và .ts
    std::string playlistName = "live.m3u8"; // Tên playlist
    int targetDurationSec = 2;          // Thời lượng mỗi phân đoạn video (segment)
    int maxFiles = 5;                   // Số segment tối đa lưu trong playlist (rolling buffer)
    int width = 1280;                   // Độ phân giải chiều rộng (khi dùng test camera)
    int height = 720;                   // Độ phân giải chiều cao (khi dùng test camera)
    int fps = 25;                       // Số khung hình/giây
};

class HlsStreamer {
public:
    HlsStreamer();
    ~HlsStreamer();

    // Tìm kiếm đường dẫn gst-launch-1.0 trên hệ thống Windows
    static std::string detectGstExecutable(const std::string& preferredPath = "");

    // Khởi chạy luồng Test Camera (videotestsrc) đẩy lên HLS local
    bool startTestStream(const StreamConfig& config = StreamConfig());

    // Khởi chạy luồng từ RTSP Camera thật đẩy lên HLS local (không tốn CPU, chỉ transmux H.264)
    bool startRtspStream(const std::string& rtspUrl, const StreamConfig& config = StreamConfig());

    // Dừng luồng streaming
    void stopStream();

    // Kiểm tra luồng có đang hoạt động hay không
    bool isStreaming() const;

    // Lấy đường dẫn file playlist .m3u8 hiện tại
    std::string getPlaylistFilePath() const;

    // Lấy chuỗi lệnh pipeline GStreamer đã sinh
    std::string getLastPipeline() const;

private:
    void* m_processHandle = nullptr;
    unsigned long m_processId = 0;
    std::string m_playlistPath;
    std::string m_lastPipeline;

    bool launchPipeline(const std::string& gstBin, const std::string& pipelineArgs);
};

} // namespace Eyerus

#endif // CAMERA_TEST_HLS_STREAMER_H

