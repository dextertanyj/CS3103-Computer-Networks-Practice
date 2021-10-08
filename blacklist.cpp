#include "blacklist.hpp"

Blacklist::Blacklist() {
	this->hostnames = new std::vector<std::string>();
}

Blacklist::~Blacklist() {
	delete this->hostnames;
}

void Blacklist::add_entries(std::vector<std::string> *entries) {
	delete this->hostnames;
	this->hostnames = entries;
};

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