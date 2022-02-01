#include "ResourceMgr.h"
#include "Factories/ResourceLoader.h"
#include "spdlog/spdlog.h"
#include "File.h"
#include "Archive.h"
#include <Utils/StringHelper.h>
#include "Lib/StormLib/StormLib.h"

namespace Ship {

	ResourceMgr::ResourceMgr(std::shared_ptr<GlobalCtx2> Context, std::string MainPath, std::string PatchesPath) : Context(Context), bIsRunning(false), FileLoadThread(nullptr) {
		OTR = std::make_shared<Archive>(MainPath, PatchesPath);

		Start();
	}

	ResourceMgr::~ResourceMgr() {
		SPDLOG_INFO("destruct ResourceMgr");
		Stop();

		FileCache.clear();
		ResourceCache.clear();
	}

	void ResourceMgr::Start() {
		const std::lock_guard<std::mutex> FileLock(FileLoadMutex);
		const std::lock_guard<std::mutex> ResLock(ResourceLoadMutex);
		if (!IsRunning()) {
			bIsRunning = true;
			FileLoadThread = std::make_shared<std::thread>(&ResourceMgr::LoadFileThread, this);
			ResourceLoadThread = std::make_shared<std::thread>(&ResourceMgr::LoadResourceThread, this);
		}
	}

	void ResourceMgr::Stop() {
		if (IsRunning()) {
			{
				const std::lock_guard<std::mutex> FileLock(FileLoadMutex);
				const std::lock_guard<std::mutex> ResLock(ResourceLoadMutex);
				bIsRunning = false;
			}
			
			FileLoadNotifier.notify_all();
			ResourceLoadNotifier.notify_all();
			FileLoadThread->join();
			ResourceLoadThread->join();

			if (!FileLoadQueue.empty()) {
				SPDLOG_DEBUG("Resource manager stopped, but has {} Files left to load.", FileLoadQueue.size());
			}

			if (!ResourceLoadQueue.empty()) {
				SPDLOG_DEBUG("Resource manager stopped, but has {} Resources left to load.", FileLoadQueue.size());
			}
		}
	}

	bool ResourceMgr::IsRunning() {
		return bIsRunning && FileLoadThread != nullptr;
	}

	void ResourceMgr::LoadFileThread() {
		SPDLOG_INFO("Resource Manager LoadFileThread started");

		while (true) {
			std::unique_lock<std::mutex> Lock(FileLoadMutex);

			while (bIsRunning && FileLoadQueue.empty()) {
				FileLoadNotifier.wait(Lock);
			}

			if (!bIsRunning) {
				break;
			}

			//Lock.lock();
			std::shared_ptr<File> ToLoad = FileLoadQueue.front();
			FileLoadQueue.pop();
			//Lock.unlock();

			SPDLOG_INFO("Loading File {} on ResourceMgr thread", ToLoad->path);
			OTR->LoadFile(ToLoad->path, true, ToLoad);
			//Lock.lock();
			FileCache[ToLoad->path] = ToLoad->bIsLoaded && !ToLoad->bHasLoadError ? ToLoad : nullptr;
			//Lock.unlock();

			ToLoad->FileLoadNotifier.notify_all();
		}

		SPDLOG_INFO("Resource Manager LoadFileThread ended");
	}

	void ResourceMgr::LoadResourceThread() {
		SPDLOG_INFO("Resource Manager LoadResourceThread started");

		while (true) {
			std::unique_lock<std::mutex> ResLock(ResourceLoadMutex);
			while (bIsRunning && ResourceLoadQueue.empty()) {
				ResourceLoadNotifier.wait(ResLock);
			}

			if (!bIsRunning) {
				break;
			}

			std::shared_ptr<ResourcePromise> ToLoad = nullptr;
			//ResLock.lock();
			ToLoad = ResourceLoadQueue.front();
			ResourceLoadQueue.pop();
			//ResLock.unlock();

			// Wait for the underlying File to complete loading
			{
				std::unique_lock<std::mutex> FileLock(ToLoad->File->FileLoadMutex);
				while (!ToLoad->File->bIsLoaded && !ToLoad->File->bHasLoadError) {
					ToLoad->File->FileLoadNotifier.wait(FileLock);
				}
			}

			SPDLOG_INFO("Loading Resource {} on ResourceMgr thread", ToLoad->File->path);
			auto UnmanagedRes = ResourceLoader::LoadResource(ToLoad->File);
			auto Res = std::shared_ptr<Resource>(UnmanagedRes);

			if (Res != nullptr) {
				ToLoad->bHasResourceLoaded = true;
				ToLoad->Resource = Res;

				SPDLOG_INFO("LOADED Resource {} on ResourceMgr thread", ToLoad->File->path);
			} else {
				ToLoad->bHasResourceLoaded = false;
				ToLoad->Resource = nullptr;

				SPDLOG_ERROR("Resource load FAILED {} on ResourceMgr thread", ToLoad->File->path);
			}

			//ResLock.lock();
			ResourceCache[Res->File->path] = Res;
			//ResLock.unlock();

			ToLoad->ResourceLoadNotifier.notify_all();
		}

		SPDLOG_INFO("Resource Manager LoadResourceThread ended");
	}

	std::shared_ptr<File> ResourceMgr::LoadFileAsync(std::string FilePath) {
		const std::lock_guard<std::mutex> Lock(FileLoadMutex);
		// File NOT already loaded...?
		if (FileCache.find(FilePath) == FileCache.end()) {
			SPDLOG_DEBUG("Cache miss on File load: {}", filePath.c_str());
			std::shared_ptr<File> ToLoad = std::make_shared<File>();
			ToLoad->path = FilePath;

			FileLoadQueue.push(ToLoad);
			FileLoadNotifier.notify_all();

			return ToLoad;
		}

		return FileCache[FilePath];
	}

	std::shared_ptr<File> ResourceMgr::LoadFile(std::string FilePath) {
		auto ToLoad = LoadFileAsync(FilePath);
		// Wait for the File to actually be loaded if we are told to block.
		std::unique_lock<std::mutex> Lock(ToLoad->FileLoadMutex);
		while (!ToLoad->bIsLoaded && !ToLoad->bHasLoadError) {
			ToLoad->FileLoadNotifier.wait(Lock);
		}

		return ToLoad;
	}

	std::shared_ptr<Resource> ResourceMgr::LoadResource(std::string FilePath) {
		auto Promise = LoadResourceAsync(FilePath);

		std::unique_lock<std::mutex> Lock(Promise->ResourceLoadMutex);
		while (!Promise->bHasResourceLoaded) {
			Promise->ResourceLoadNotifier.wait(Lock);
		}

		return Promise->Resource;
	}

	std::shared_ptr<ResourcePromise> ResourceMgr::LoadResourceAsync(std::string FilePath) {
		FilePath = StringHelper::Replace(FilePath, "/", "\\");

		if (StringHelper::StartsWith(FilePath, "__OTR__"))
			FilePath = StringHelper::Split(FilePath, "__OTR__")[1];

		if (StringHelper::StartsWith(FilePath, "spot00_room"))
		{
			int bp = 0;
		}

		std::shared_ptr<File> FileData = LoadFile(FilePath);
		std::shared_ptr<ResourcePromise> Promise = std::make_shared<ResourcePromise>();
		Promise->File = FileData;

		const std::lock_guard<std::mutex> ResLock(ResourceLoadMutex);
		if (ResourceCache.find(FilePath) == ResourceCache.end() || ResourceCache[FilePath]->isDirty/* || !FileData->bIsLoaded*/) {
			if (ResourceCache.find(FilePath) == ResourceCache.end()) {
				SPDLOG_DEBUG("Cache miss on Resource load: {}", filePath.c_str());
			}

			Promise->bHasResourceLoaded = false;
			ResourceLoadQueue.push(Promise);
			ResourceLoadNotifier.notify_all();
		} else {
			Promise->bHasResourceLoaded = true;
			Promise->Resource = ResourceCache[FilePath];
		}

		return Promise;
	}

	std::shared_ptr<std::vector<std::shared_ptr<ResourcePromise>>> ResourceMgr::CacheDirectoryAsync(std::string SearchMask) {
		auto loadedList = std::make_shared<std::vector<std::shared_ptr<ResourcePromise>>>();
		auto fileList = OTR->ListFiles(SearchMask);

		for (DWORD i = 0; i < fileList.size(); i++) {
			auto file = LoadResourceAsync(fileList.operator[](i).cFileName);
			if (file != nullptr) {
				loadedList->push_back(file);
			}
		}

		return loadedList;
	}

	std::shared_ptr<std::vector<std::shared_ptr<Resource>>> ResourceMgr::CacheDirectory(std::string SearchMask) {
		auto PromiseList = CacheDirectoryAsync(SearchMask);
		auto LoadedList = std::make_shared<std::vector<std::shared_ptr<Resource>>>();

		for (int32_t i = 0; i < PromiseList->size(); i++) {
			auto Promise = PromiseList->at(i);

			std::unique_lock<std::mutex> Lock(Promise->ResourceLoadMutex);
			while (!Promise->bHasResourceLoaded) {
				Promise->ResourceLoadNotifier.wait(Lock);
			}

			LoadedList->push_back(Promise->Resource);
		}

		return LoadedList;
	}

	std::shared_ptr<std::vector<std::shared_ptr<Resource>>> ResourceMgr::DirtyDirectory(std::string SearchMask) 
	{
		auto PromiseList = CacheDirectoryAsync(SearchMask);
		auto LoadedList = std::make_shared<std::vector<std::shared_ptr<Resource>>>();

		for (int32_t i = 0; i < PromiseList->size(); i++) {
			auto Promise = PromiseList->at(i);

			std::unique_lock<std::mutex> Lock(Promise->ResourceLoadMutex);
			while (!Promise->bHasResourceLoaded) {
				Promise->ResourceLoadNotifier.wait(Lock);
			}

			if (Promise->Resource != nullptr)
				Promise->Resource->isDirty = true;

			LoadedList->push_back(Promise->Resource);
		}

		return LoadedList;
	}

	void ResourceMgr::InvalidateResourceCache() {
		ResourceCache.empty();
	}

	std::string ResourceMgr::HashToString(uint64_t Hash) {
		return OTR->HashToString(Hash);
	}
}