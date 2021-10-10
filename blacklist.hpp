#ifndef HTTPS_PROXY_BLACKLIST_HPP_
#define HTTPS_PROXY_BLACKLIST_HPP_

#include <memory>
#include <string>
#include <vector>

class Blacklist {
	public:
		Blacklist();
		void add_entries(std::unique_ptr<std::vector<std::string>>);
		void add_entry(std::string);
		bool is_blocked(std::string);
	private:
		std::unique_ptr<std::vector<std::string>> hostnames;
};

#endif  // HTTPS_PROXY_BLACKLIST_HPP_
