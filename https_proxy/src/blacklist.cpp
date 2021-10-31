#include "blacklist.hpp"

#include <memory>

Blacklist::Blacklist() : hostnames(std::make_unique<std::vector<std::string>>()) {
}

void Blacklist::add_entries(std::unique_ptr<std::vector<std::string>> entries) {
	this->hostnames = std::move(entries);
}

void Blacklist::add_entry(std::string entry) {
	this->hostnames->push_back(entry);
};

bool Blacklist::is_blocked(std::string hostname) {
	for (std::vector<std::string>::iterator iter = this->hostnames->begin();
		iter != this->hostnames->end(); iter++) {
			if (hostname.find(*iter) != std::string::npos) {
				return true;
			}
		}
	return false;
};