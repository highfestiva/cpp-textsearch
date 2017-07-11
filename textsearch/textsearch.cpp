// textsearch.cpp : Defines the entry point for the console application.
//

#include <SDKDDKVer.h>
#include <windows.h>
#include <Shlwapi.h>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <thread>

#pragma comment(lib, "Shlwapi.lib")
using namespace std;


struct FileEntry {
	wstring filename;
	int size;
	FileEntry() {}
	FileEntry(wstring _filename, int _size) :
		filename(_filename),
		size(_size) {}
};


const auto excludedirs_ = set<wstring>({ L".",L"..",L".git",L".svn",L"__pycache__" });
const auto excludeexts_ = set<string>({ "exe","bin","png","jpg","gif","tga","ppm","ico","icns","psd","wav","mp3","ttf","afm","eot","zip","7z","gz","jar","pyc","pyo","pyd","whl","class","war","dll","obj","pch","pdb","ilk","suo" });
const size_t maxfilesize_ = 1024 * 1024;

string search_;
mutex qmutex_;
condition_variable qcondition_;
queue<FileEntry> workq_;


void search_thread_entry() {
	char* buf = (char*)malloc(maxfilesize_);
	FileEntry f;
	for (;;) {
		{
			std::unique_lock<decltype(qmutex_)> lock(qmutex_);
			while (workq_.empty()) {
				qcondition_.wait(lock);
			}
			f = workq_.front();
			workq_.pop();
		}
		if (f.filename.empty()) {
			break;
		}
		ifstream fs(f.filename.c_str(), ios::binary);
		fs.read(buf, f.size);
		string s(buf, f.size);
		size_t index = 0;
		int len = f.size;
		for (;;) {
			index = s.find(search_, index);
			if (index == string::npos) {
				break;
			}
			int lo = index - 10;
			int hi = index + search_.length() + 10;
			lo = lo < 0 ? 0 : lo;
			hi = hi >= len ? len : hi;
			string found = s.substr(lo, hi-lo);
			replace(found.begin(), found.end(), '\n', ' ');
			replace(found.begin(), found.end(), '\r', ' ');
			string fn(f.filename.begin(), f.filename.end());
			replace(fn.begin(), fn.end(), '\\', '/');
			cout << fn << ": " << found << endl;
			index += search_.length();
		}
	}
}


int workdir(wchar_t* dir) {
	int filecnt = 0;
	wchar_t path[MAX_PATH];
	WIN32_FIND_DATA entry;
	HANDLE dir_handle;
	// first we are going to process any subdirectories
	PathCombine(path, dir, L"*");
	try {
		dir_handle = FindFirstFile(path, &entry);
		if (dir_handle != INVALID_HANDLE_VALUE) {
			do {
				if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					if (excludedirs_.find(entry.cFileName) != excludedirs_.end()) {
						continue;
					}
					PathCombine(path, dir, entry.cFileName);
					filecnt += workdir(path);
				} else {
					if (entry.nFileSizeLow < search_.length() || entry.nFileSizeLow > maxfilesize_) {
						continue;
					}
					wstring wname = entry.cFileName;
					const size_t i = wname.find_last_of('.');
					if (i != std::string::npos)
					{
						string ext(wname.begin()+i+1, wname.end());
						if (excludeexts_.find(ext) != excludeexts_.end()) {
							continue;
						}
					}
					PathCombine(path, dir, entry.cFileName);
					std::unique_lock<decltype(qmutex_)> lock(qmutex_);
					workq_.push(FileEntry(path, entry.nFileSizeLow));
					qcondition_.notify_one();
					++filecnt;
				}
			} while (FindNextFile(dir_handle, &entry));
			FindClose(dir_handle);
		}
	} catch (...) {
	}
	return filecnt;
}


int main(int argc, char* argv[]) {
	stringstream cls;
	for (int i = 1; i < argc; ++i) {
		if (i > 1) {
			cls << ' ';
		}
		cls << argv[i];
	}
	search_ = cls.str();
	if (search_.empty()) {
		return 1;
	}
	list<thread> threads;
	for (int i = 0; i < 3; ++i) {
		threads.push_back(thread(search_thread_entry));
	}
	int filecnt = workdir(L".");
	{
		std::unique_lock<decltype(qmutex_)> lock(qmutex_);
		for (auto t = threads.begin(); t != threads.end(); ++t) {
			workq_.push(FileEntry(L"", -1));
			qcondition_.notify_one();
		}
	}
	for (auto t = threads.begin(); t != threads.end(); ++t) {
		t->join();
	}
	cout << "Searched " << filecnt << " files." << endl;
    return 0;
}
