#include <Common/Logger/logger.h>
#include <Common/Util/util.h>
#include <Common/Time/timeutil.h>
#include <Common/config.h>
#ifdef _WIN32
#include <nanomsg/src/nn.h>
#include <nanomsg/src/pubsub.h>
#else
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#endif

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using std::lock_guard;

namespace EliteQuant
{
	logger* logger::pinstance_ = nullptr;
	mutex logger::instancelock_;

	logger::logger() : logfile(nullptr) {
		Initialize();
	}

	logger::~logger() {
		fclose(logfile);
	}

	logger& logger::instance() {
		if (pinstance_ == nullptr) {
			lock_guard<mutex> g(instancelock_);
			if (pinstance_ == nullptr) {
				pinstance_ = new logger();
			}
		}
		return *pinstance_;
	}

	void logger::Initialize() {
		string fname;
		if (CConfig::instance()._mode == RUN_MODE::REPLAY_MODE) {
			fname = CConfig::instance().logDir() + "/elitequant-replay-" + ymd() + "..txt";
		}
		else {
			fname = CConfig::instance().logDir() + "/elitequant-" + ymd() + "..txt";
		}

		logfile = fopen(fname.c_str(), "w");
		setvbuf(logfile, nullptr, _IONBF, 0);
	}

	void logger::Printf2File(const char *format, ...) {
		lock_guard<mutex> g(instancelock_);

		static char buf[1024 * 2];
		string tmp = nowMS();
		size_t sz = tmp.size();
		strcpy(buf, tmp.c_str());
		buf[sz] = ' ';

		va_list args;
		va_start(args, format);
		vsnprintf(buf + sz + 1, 1024 * 2 - sz - 1, format, args);
		size_t buflen = strlen(buf);
		fwrite(buf, sizeof(char), buflen, logfile);
		va_end(args);
	}
}
