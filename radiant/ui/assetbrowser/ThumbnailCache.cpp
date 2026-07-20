#include "ThumbnailCache.h"

#include "ifilesystem.h"
#include "imodule.h"
#include "os/dir.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include <wx/filefn.h>

namespace ui
{

namespace
{

constexpr int EVENT_LOADED = 1;
constexpr int EVENT_NEEDS_RENDER = 2;

std::string fnv1aHex(const std::string& input)
{
    std::uint64_t hash = 14695981039346656037ull;

    for (unsigned char c : input)
    {
        hash ^= c;
        hash *= 1099511628211ull;
    }

    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));

    return buffer;
}

}

ThumbnailCache::ThumbnailCache()
{
    _cacheDir = module::GlobalModuleRegistry().getApplicationContext().getCacheDataPath()
        + "thumbnails/";

    os::makeDirectory(_cacheDir);

    Bind(wxEVT_THREAD, &ThumbnailCache::onWorkerEvent, this);

    _worker = std::thread(&ThumbnailCache::workerLoop, this);
}

ThumbnailCache::~ThumbnailCache()
{
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _shutdown = true;
    }

    _queueCondition.notify_one();
    _worker.join();
}

void ThumbnailCache::setThumbnailSize(int size)
{
    _thumbnailSize = size;
}

const wxBitmap* ThumbnailCache::find(const std::string& key)
{
    auto found = _bitmaps.find(key);

    return found != _bitmaps.end() ? &found->second : nullptr;
}

void ThumbnailCache::request(const std::string& key, const std::string& sourceVfsPath)
{
    if (_bitmaps.count(key) > 0 || _pending.count(key) > 0 || _failed.count(key) > 0) return;

    _pending.insert(key);

    queueJob({ Job::Type::Load, key, wxImage(), sourceVfsPath, _generation });
}

void ThumbnailCache::dropPending(const std::string& key)
{
    _pending.erase(key);
}

void ThumbnailCache::markFailed(const std::string& key)
{
    _pending.erase(key);
    _failed.insert(key);
}

void ThumbnailCache::invalidate()
{
    ++_generation;

    _bitmaps.clear();
    _pending.clear();
    _failed.clear();
    _renderQueue.clear();
}

void ThumbnailCache::clearAll()
{
    invalidate();

    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _jobs.clear();
        _jobs.push_back({ Job::Type::Clear });
    }

    _queueCondition.notify_one();
}

std::string ThumbnailCache::popNextRenderKey()
{
    if (_renderQueue.empty()) return {};

    auto key = _renderQueue.front();
    _renderQueue.pop_front();

    return key;
}

void ThumbnailCache::pushRenderKeyFront(const std::string& key)
{
    _renderQueue.push_front(key);
}

bool ThumbnailCache::hasPendingRenders() const
{
    return !_renderQueue.empty();
}

void ThumbnailCache::storeRendered(const std::string& key, const wxImage& image)
{
    queueJob({ Job::Type::Save, key, image.Copy(), std::string(), _generation });
}

sigc::signal<void, const std::string&>& ThumbnailCache::signal_thumbnailLoaded()
{
    return _thumbnailLoaded;
}

sigc::signal<void>& ThumbnailCache::signal_renderRequested()
{
    return _renderRequested;
}

void ThumbnailCache::queueJob(Job job)
{
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _jobs.emplace_back(std::move(job));
    }

    _queueCondition.notify_one();
}

void ThumbnailCache::workerLoop()
{
    while (true)
    {
        Job job;

        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _queueCondition.wait(lock, [this] { return _shutdown || !_jobs.empty(); });

            if (_shutdown) return;

            job = std::move(_jobs.front());
            _jobs.pop_front();
        }

        if (job.type == Job::Type::Clear)
        {
            os::forEachItemInDirectory(_cacheDir,
                [](const fs::path& item) { fs::remove(item); }, std::nothrow);
            continue;
        }

        auto file = cacheFileForKey(job.key);

        if (job.type == Job::Type::Save)
        {
            auto scaled = scaleToThumbnail(job.image);
            scaled.SaveFile(file, wxBITMAP_TYPE_PNG);

            postToMainThread(EVENT_LOADED, job.key, scaled, job.generation);
        }
        else if (wxFileExists(file) && !cachedFileIsStale(job.sourcePath, file))
        {
            wxImage image;

            if (image.LoadFile(file, wxBITMAP_TYPE_PNG))
            {
                postToMainThread(EVENT_LOADED, job.key, image, job.generation);
            }
            else
            {
                postToMainThread(EVENT_NEEDS_RENDER, job.key, wxImage(), job.generation);
            }
        }
        else
        {
            postToMainThread(EVENT_NEEDS_RENDER, job.key, wxImage(), job.generation);
        }
    }
}

std::string ThumbnailCache::cacheFileForKey(const std::string& key) const
{
    return _cacheDir + fnv1aHex(key) + ".png";
}

bool ThumbnailCache::cachedFileIsStale(const std::string& sourceVfsPath, const std::string& cacheFile) const
{
    if (sourceVfsPath.empty()) return false;

    auto info = GlobalFileSystem().getFileInfo(sourceVfsPath);

    if (info.isEmpty()) return false;

    auto sourceFile = info.getPhysicalPath();

    if (sourceFile.empty()) return false;

    auto sourceTime = wxFileModificationTime(sourceFile);
    auto cacheTime = wxFileModificationTime(cacheFile);

    return sourceTime > 0 && (cacheTime <= 0 || sourceTime > cacheTime);
}

wxImage ThumbnailCache::scaleToThumbnail(wxImage image) const
{
    int width = image.GetWidth();
    int height = image.GetHeight();

    if (width <= 0 || height <= 0) return image;

    int target = _thumbnailSize;
    int square = std::min(width, height);

    if (width != height)
    {
        image = image.GetSubImage(wxRect(
            (width - square) / 2, (height - square) / 2, square, square));
    }

    if (square != target)
    {
        image.Rescale(target, target, wxIMAGE_QUALITY_HIGH);
    }

    return image;
}

void ThumbnailCache::postToMainThread(int id, const std::string& key, const wxImage& image, int generation)
{
    auto* ev = new wxThreadEvent(wxEVT_THREAD);

    ev->SetInt(id);
    ev->SetString(key);
    ev->SetExtraLong(generation);

    if (image.IsOk())
    {
        ev->SetPayload(image.Copy());
    }

    wxQueueEvent(this, ev);
}

void ThumbnailCache::onWorkerEvent(wxThreadEvent& ev)
{
    if (ev.GetExtraLong() != _generation) return;

    auto key = ev.GetString().ToStdString();

    if (ev.GetInt() == EVENT_LOADED)
    {
        _bitmaps[key] = wxBitmap(ev.GetPayload<wxImage>());
        _pending.erase(key);

        _thumbnailLoaded.emit(key);
    }
    else
    {
        _renderQueue.push_back(key);

        _renderRequested.emit();
    }
}

}
