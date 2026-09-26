#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include "hls_streamer.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>

namespace Eyerus
{

    namespace
    {

        bool fileExists(const std::string &path)
        {
            DWORD dwAttrib = GetFileAttributesA(path.c_str());
            return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
        }

        void ensureDirectoryExists(const std::string &dir)
        {
            CreateDirectoryA(dir.c_str(), NULL);
        }

    } // anonymous namespace

    HlsStreamer::HlsStreamer() : m_processHandle(nullptr), m_processId(0)
    {
    }

    HlsStreamer::~HlsStreamer()
    {
        stopStream();
    }

    std::string HlsStreamer::detectGstExecutable(const std::string &preferredPath)
    {
        if (!preferredPath.empty() && fileExists(preferredPath))
        {
            return preferredPath;
        }

        // Kiem tra bien moi truong he thong
        char envBuf[1024] = {0};
        if (GetEnvironmentVariableA("GSTREAMER_1_0_ROOT_MSVC_X86_64", envBuf, sizeof(envBuf)) > 0)
        {
            std::string p = std::string(envBuf) + "bin\\gst-launch-1.0.exe";
            if (fileExists(p))
                return p;
        }
        if (GetEnvironmentVariableA("GSTREAMER_1_0_ROOT_MINGW_X86_64", envBuf, sizeof(envBuf)) > 0)
        {
            std::string p = std::string(envBuf) + "bin\\gst-launch-1.0.exe";
            if (fileExists(p))
                return p;
        }
        if (GetEnvironmentVariableA("GST_PATH", envBuf, sizeof(envBuf)) > 0)
        {
            std::string p = envBuf;
            if (fileExists(p))
                return p;
        }

        // Cac duong dan mac dinh pho bien tren Windows
        std::vector<std::string> candidates = {
            "C:\\gstreamer\\1.0\\msvc_x86_64\\bin\\gst-launch-1.0.exe",
            "C:\\gstreamer\\1.0\\mingw_x86_64\\bin\\gst-launch-1.0.exe",
            "C:\\gstreamer\\1.0\\x86_64\\bin\\gst-launch-1.0.exe",
            "D:\\gstreamer\\1.0\\msvc_x86_64\\bin\\gst-launch-1.0.exe",
            "D:\\gstreamer\\1.0\\mingw_x86_64\\bin\\gst-launch-1.0.exe",
            "C:\\tools\\gstreamer\\bin\\gst-launch-1.0.exe"};

        for (const auto &path : candidates)
        {
            if (fileExists(path))
            {
                return path;
            }
        }

        // Mac dinh gia dinh gst-launch-1.0 co san trong PATH
        return "gst-launch-1.0";
    }

    bool HlsStreamer::launchPipeline(const std::string &gstBin, const std::string &pipelineArgs)
    {
        stopStream();

        m_lastPipeline = gstBin + " " + pipelineArgs;

        std::string cmd = "\"" + gstBin + "\" " + pipelineArgs;

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        std::vector<char> cmdBuffer(cmd.begin(), cmd.end());
        cmdBuffer.push_back('\0');

        BOOL success = CreateProcessA(
            NULL,
            cmdBuffer.data(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi);

        if (!success)
        {
            return false;
        }

        m_processHandle = pi.hProcess;
        m_processId = pi.dwProcessId;
        CloseHandle(pi.hThread);

        return true;
    }

    bool HlsStreamer::startTestStream(const StreamConfig &config)
    {
        std::string gstBin = detectGstExecutable(config.gstExecutable);

        ensureDirectoryExists(config.outputDir);

        m_playlistPath = config.outputDir + "/" + config.playlistName;

        std::string segmentPattern = config.outputDir + "/segment_%05d.ts";

        std::ostringstream args;
        args << "-v videotestsrc is-live=true pattern=ball ! "
             << "video/x-raw,framerate=" << config.fps << "/1,width=" << config.width << ",height=" << config.height << " ! "
             << "timeoverlay ! "
             << "x264enc tune=zerolatency speed-preset=ultrafast bitrate=1500 key-int-max=" << (config.fps * 2) << " ! "
             << "h264parse ! "
             << "hlssink location=\"" << segmentPattern << "\" playlist-location=\"" << m_playlistPath << "\" "
             << "target-duration=" << config.targetDurationSec << " max-files=" << config.maxFiles;

        return launchPipeline(gstBin, args.str());
    }

    bool HlsStreamer::startRtspStream(const std::string &rtspUrl, const StreamConfig &config)
    {
        std::string gstBin = detectGstExecutable(config.gstExecutable);

        ensureDirectoryExists(config.outputDir);

        m_playlistPath = config.outputDir + "/" + config.playlistName;

        std::string segmentPattern = config.outputDir + "/segment_%05d.ts";

        std::ostringstream args;
        args << "-v rtspsrc location=\"" << rtspUrl << "\" latency=200 ! "
             << "rtph264depay ! "
             << "h264parse ! "
             << "hlssink location=\"" << segmentPattern << "\" playlist-location=\"" << m_playlistPath << "\" "
             << "target-duration=" << config.targetDurationSec << " max-files=" << config.maxFiles;

        return launchPipeline(gstBin, args.str());
    }

    void HlsStreamer::stopStream()
    {
        if (m_processHandle)
        {
            TerminateProcess(static_cast<HANDLE>(m_processHandle), 0);
            CloseHandle(static_cast<HANDLE>(m_processHandle));
            m_processHandle = nullptr;
            m_processId = 0;
        }
    }

    bool HlsStreamer::isStreaming() const
    {
        if (!m_processHandle)
            return false;
        DWORD exitCode = 0;
        if (GetExitCodeProcess(static_cast<HANDLE>(m_processHandle), &exitCode))
        {
            return (exitCode == STILL_ACTIVE);
        }
        return false;
    }

    std::string HlsStreamer::getPlaylistFilePath() const
    {
        return m_playlistPath;
    }

    std::string HlsStreamer::getLastPipeline() const
    {
        return m_lastPipeline;
    }

} // namespace Eyerus
