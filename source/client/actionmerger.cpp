#include <settings.hpp>
#include <server/include/protocol.hpp>
#include <utilities/include/exceptions.hpp>
#include <utilities/include/fsutil.hpp>
#include <utilities/include/debug.hpp>
#include <utilities/include/strings.hpp>
#include <utilities/include/fsutil.hpp>

#include <functional>
#include <unordered_map>
#include <string>
#include <Windows.h>
#include <limits>
#include <actionmerger.hpp>
#include <log.hpp>
#include <Pathcch.h>

using namespace std;
using namespace client;
using namespace server;
using namespace utilities;

opcode client::get_flag_bit(DWORD event, DWORD flags) {
	switch (event) {
	case FILE_ACTION_ADDED:
		if (flags & FILE_NOTIFY_CHANGE_DIR_NAME)
			return INVALID;
		return CREATE;
	case FILE_ACTION_REMOVED:
		return REMOVE;
	case FILE_ACTION_MODIFIED:
		if (flags & FILE_NOTIFY_CHANGE_DIR_NAME)
			return INVALID;
		return WRITE;
	case FILE_ACTION_RENAMED_OLD_NAME:
	case FILE_ACTION_RENAMED_NEW_NAME:
		return MOVE;
	}
	return INVALID;
}

action_merger::action_merger(size_t estimatedFileNum) :
		map(estimatedFileNum), open(true) {

	// Qui impedisco il rehash per mantenere gli iteratori validi.
	map.max_load_factor(std::numeric_limits<float>::infinity());

	it = map.begin();
}

void action_merger::add_change(std::wstring& fileName, file_action& action) {
	LOGF;

	unique_lock<mutex> guard(lock);

	LOGD("--fileName: "<< utf8_encode(fileName)<< " action: "<< (int)action.op_code);

	if (open) {
		if (map.count(fileName)) {

			file_action& up = map[fileName];
			int old_pending = __builtin_popcount( (int8_t)up.op_code & ~(WRITE|APPLY) );
			up.op_code |= action.op_code;
			pending_count.fetch_add(__builtin_popcount( (int8_t)up.op_code & ~(WRITE|APPLY) ));
			pending_count.fetch_sub(old_pending);

			LOGD("Pending count up: " << pending_count);

			for (int i = 0; i < 8; i++)
			if (CompareFileTime(&action.timestamps[i], &up.timestamps[i])
					== 1)
			up.timestamps[i] = action.timestamps[i];
		}
		else {
			map[fileName] = action;
			pending_count.fetch_add(__builtin_popcount( (int8_t)action.op_code & ~(WRITE|APPLY) ));
			LOGD("Pending count up: " << action_merger::inst().pending_count);
		}
	}

	cv.notify_all();
}

void action_merger::add_change(const change_entity& che) {
	LOGF;

	unique_lock<mutex> guard(lock);
	wstring name(che->FileName, che->FileNameLength / sizeof(wchar_t)), path = settings::inst().watched_dir.value + name;
	opcode flag = get_flag_bit(che->Action, che.flags);

	bool dirb=isPathDir(path.c_str());
	if(flag == INVALID ||(flag == WRITE && dirb)) {
		return;
	}

	if(flag == WRITE) {
		//TODO

		wstringstream ss;
		ss << settings::inst().temp_dir.value << name << L'.' << hex << che.time;
		wstring tempPath = ss.str();

		//Se il file era in una sottodirectory creo lo stesso albero anche in temp dir
		wstring dir = dirName(name);
		if(dir != L"") {
			wstring dirPath(settings::inst().temp_dir.value + dir);
			createDirectoryRecursively(dirPath.c_str());
		}
		if(CreateHardLinkW(tempPath.c_str(), path.c_str(), NULL) != 0)
			//HARDLYNK
			LOGD("--HARDLINK created--");
		else
			//COPY
			if(!CopyFileW(path.c_str(), tempPath.c_str(), false)){
				LOGD("Unable to fulfill write... skipping!");
				return;
			}
	}

	log::inst().issue(che);

	if (open) {
		LOGD("+fileName: "<< utf8_encode(path) << " action: "<< (int)flag);

		if((flag != WRITE) && (flag!= APPLY))
			++pending_count;

		LOGD("Pending count up: " << pending_count);

		file_action& fa = map[name];
		fa.op_code |= flag;

		if (che->Action == FILE_ACTION_RENAMED_OLD_NAME) {
			change_entity newNameEntity =
			utilities::shared_queue<change_entity>::inst().dequeue();
			if (newNameEntity->Action != FILE_ACTION_RENAMED_NEW_NAME)
			throw fs_exception(
					"Wrong action order \"FILE_ACTION_RENAMED_NEW_NAME\" expected.",
					__LINE__, __func__, __FILE__);

			log::inst().issue(newNameEntity);
			fa.newName = wstring(newNameEntity->FileName,
					newNameEntity->FileNameLength / sizeof(wchar_t));
		}

		// __builtin_ffs restituisce la posizione del primo bit meno significativo a 1
		// FIXME: sto indicizzando bene? (si)
//		if(flag!=WRITE)
			fa.timestamps[__builtin_ffs(flag) - 1] = che.time;
//		else{
//
//			fa.timestamps[7] = che.time;
//		}
		cv.notify_all();
	}
	// FIXME: qui devo lanciare una eccezione?
}

file_action& file_action::operator |=(const log_entry_header& entry) {
	LOGF;
	op_code |= entry.op_code;
	LOGD("Index: " << __builtin_ffs(entry.op_code) - 1);
	FILETIME& time = timestamps[__builtin_ffs(entry.op_code) - 1];
	if (CompareFileTime(&entry.timestamp, &time) == 1)
	time = entry.timestamp;
	return *this;
}

file_action& file_action::operator ^=(const log_entry_header& entry) {

	LOGD(
			"opcode: "<< (int)op_code << " & entry.opcode: " << (int)entry.op_code);
	for (uint8_t i = 0, j = 1; i < 8; i++, j = j << 1) {
		if ((op_code & j) && (entry.op_code & j)) {

			LOGD("type: "<< (int)j);
			FILETIME& _t = timestamps[i];
			if (CompareFileTime(&entry.timestamp, &_t) >= 0) {
				LOGD("compare Ok");
				_t = {0};
				op_code ^= j;
			}
			LOGD("opcode: "<< (int)op_code);
		}
	}
	return *this;
}

bool action_merger::peek(){
	LOGF;
	unique_lock<mutex> guard(lock);

	// Per poter rimuovere qualcosa devo sempre aver qualcosa da rimuovere..
	cv.wait(guard, [this]() {
				return !map.empty() || (!open);
			});

	LOGD("Map size merger: " << map.size());

	// Se stiamo chiudendo mi fermo...
	return !map.empty();
}

bool action_merger::remove(std::wstring& name, file_action& actions) {
	LOGF;
	unique_lock<mutex> guard(lock);

	LOGD("Map size merger: " << map.size());

	// Se stiamo chiudendo mi fermo...
	if (map.empty())
		return false;

	if(it==map.end())
		it = map.begin();

	// Ora prelevo le informazioni perchè sono sicuro che il mio iteratore
	// sia valido (no rehash + size > 0 + no end())

	name = it->first;
	actions = it->second;

	// Next one
	it = map.erase(it);
	LOGD("Map size merger: "<<map.size());
	return true;
}

void action_merger::close() {
	lock_guard<mutex> guard(lock);
	open = false;
	cv.notify_all();
}
