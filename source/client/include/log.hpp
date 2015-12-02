/*
 * log.hpp
 *
 *  Created on: 13 nov 2015
 *      Author: Marco
 */

#ifndef SOURCE_CLIENT_INCLUDE_LOG_HPP_
#define SOURCE_CLIENT_INCLUDE_LOG_HPP_

#include <directorylistener.hpp>
#include <utilities/include/singleton.hpp>

#include <fstream>

namespace client {

class log : public utilities::singleton<log> {
	friend class utilities::singleton<log>;
	log();
	std::wofstream log_file;
	~log();
public:

	template <typename T>
	void issue(const T& val) {
		log_file << L"i " << val << std::endl;
	}

	template <typename T>
	void close(const T& val){
		log_file << L"c " << val << std::endl;
	}
};

} /* namespace client */

#endif /* SOURCE_CLIENT_INCLUDE_LOG_HPP_ */
