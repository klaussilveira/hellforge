#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include <wx/bitmap.h>
#include <wx/event.h>
#include <wx/image.h>

#include <sigc++/signal.h>

namespace ui
{

class ThumbnailCache :
    public wxEvtHandler
{
public:
    ThumbnailCache();
    ~ThumbnailCache() override;

    void setThumbnailSize(int size);

    const wxBitmap* find(const std::string& key);
    void request(const std::string& key, const std::string& sourceVfsPath = std::string());
    void dropPending(const std::string& key);
    void markFailed(const std::string& key);
    void invalidate();
    void clearAll();

    std::string popNextRenderKey();
    void pushRenderKeyFront(const std::string& key);
    bool hasPendingRenders() const;
    void storeRendered(const std::string& key, const wxImage& image);

    sigc::signal<void, const std::string&>& signal_thumbnailLoaded();
    sigc::signal<void>& signal_renderRequested();

private:
    struct Job
    {
        enum class Type { Load, Save, Clear };

        Type type = Type::Load;
        std::string key;
        wxImage image;
        std::string sourcePath;
        int generation = 0;
    };

    void workerLoop();
    std::string cacheFileForKey(const std::string& key) const;
    bool cachedFileIsStale(const std::string& sourceVfsPath, const std::string& cacheFile) const;
    wxImage scaleToThumbnail(wxImage image) const;
    void queueJob(Job job);
    void postToMainThread(int id, const std::string& key, const wxImage& image, int generation);
    void onWorkerEvent(wxThreadEvent& ev);

    std::string _cacheDir;
    std::atomic<int> _thumbnailSize{ 128 };
    int _generation = 0;

    std::map<std::string, wxBitmap> _bitmaps;
    std::set<std::string> _pending;
    std::set<std::string> _failed;
    std::deque<std::string> _renderQueue;

    std::mutex _queueMutex;
    std::condition_variable _queueCondition;
    std::deque<Job> _jobs;
    bool _shutdown = false;
    std::thread _worker;

    sigc::signal<void, const std::string&> _thumbnailLoaded;
    sigc::signal<void> _renderRequested;
};

}
