#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <bitset>
#include <chrono>
#include <csignal>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <queue>
#include <functional>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>
#include <deque>
#include <algorithm>

#include "logger/logger.hpp"

#define KNOWN_SIZE 1
#define UNKNOWN_SIZE 0

double scheduling_count = 0;
double total_scheduling_time = 0;

Logger logger = Logger("./load_balancer.log");

class Request : public std::enable_shared_from_this<Request> {
    public:
        Request(std::string, int);
        std::string get_name();
        std::shared_ptr<Request> start();
        std::shared_ptr<Request> complete();
        int get_token();
        int get_size();
        double get_arrival_time();
        double get_service_time();
    private:
        std::string name;
        int size;
        int token;
        std::chrono::system_clock::time_point arrival_time;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point completion_time;
};

Request::Request(std::string request_name, int request_size) {
    this->arrival_time = std::chrono::system_clock::now();
    this->name = request_name;
    this->size = request_size;
    this->token = std::rand() % 2;
}

using RequestPtr = std::shared_ptr<Request>;

std::string Request::get_name() {
    return this->name;
}

std::shared_ptr<Request> Request::start() {
    this->start_time = std::chrono::system_clock::now();
    return shared_from_this();
}

std::shared_ptr<Request> Request::complete() {
    this->completion_time = std::chrono::system_clock::now();
    return shared_from_this();
}

int Request::get_size() {
    return this->size;
}

int Request::get_token() {
    return this->token;
}

double Request::get_arrival_time() {
    if (this->arrival_time == std::chrono::system_clock::time_point()) {
        return -1;
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(this->arrival_time.time_since_epoch()).count();
}

double Request::get_service_time() {
    if (this->completion_time == std::chrono::system_clock::time_point()) {
        return -1;
    }
    if (this->start_time == std::chrono::system_clock::time_point()) {
        return -1;
    }
    double service_time = std::chrono::duration_cast<std::chrono::milliseconds>(this->completion_time - this->start_time).count();
    return service_time;
}

bool operator<(RequestPtr a, RequestPtr b) {
    return a->get_arrival_time() < b->get_arrival_time();
}

class Average {
    public:
        Average();
        double record(double);
        double query();
        bool is_valid();
    private:
        int count;
        double total;
};

Average::Average() {
    this->count = 0;
    this->total = 0;
}

double Average::record(double value) {
    this->count++;
    this->total += value;
    return this->query();
}

double Average::query() {
    if (!this->is_valid()) {
        return -1;
    }
    return this->total / this->count;
}

bool Average::is_valid() {
    return this->count != 0;
}

class ServerStatistic : public std::enable_shared_from_this<ServerStatistic> {
    public:
        ServerStatistic(std::string);
        std::shared_ptr<ServerStatistic> record_request(RequestPtr);
        std::string get_name();
        double get_performance_metric();
        double get_capacity();
        bool is_calibrated();
    private:
        std::string name;
        Average capacity;
        Average performance_metric;
};

using ServerPtr = std::shared_ptr<ServerStatistic>;

ServerStatistic::ServerStatistic(std::string server_name) {
    this->name = server_name;
    this->capacity = Average();
    this->performance_metric = Average();
}

std::string ServerStatistic::get_name() {
    return this->name;
}

std::shared_ptr<ServerStatistic> ServerStatistic::record_request(RequestPtr request) {
    int request_size = request->get_size();
    if (request_size > 0) {
        this->capacity.record(request->get_service_time() / request_size);
    }
    this->performance_metric.record(request->get_service_time());
    return shared_from_this();
}

bool ServerStatistic::is_calibrated() {
    return this->capacity.is_valid();
}

bool operator<(ServerPtr a, ServerPtr b) {
    double a_capacity = a->get_capacity();
    double b_capacity = b->get_capacity();
    if (a_capacity == -1 || b_capacity == -1) {
        double a_performance_metric = a->get_performance_metric();
        double b_performance_metric = b->get_performance_metric();
        return a_performance_metric < b_performance_metric;
    }
    return a_capacity < b_capacity;
}

double ServerStatistic::get_performance_metric() {
    return this->performance_metric.query();
}

double ServerStatistic::get_capacity() {
    return this->capacity.query();
}


// Function declarations
std::vector<std::string> parseWithDelimiter(std::string, std::string);
std::vector<std::string> parseServernames(char*, int);
std::string parser_filename(std::string);
int parser_jobsize(std::string);
std::string scheduleJobToServer(ServerPtr, RequestPtr);

class LoadBalancer {
    public:
        LoadBalancer(std::vector<std::string>);
        void handle_completion(std::string);
        void handle_request(std::string);
        std::string handle_next();
    private:
        int last_sent;
        std::priority_queue<ServerPtr, std::vector<ServerPtr>, std::greater<ServerPtr>> calibrated_servers;
        std::priority_queue<ServerPtr, std::vector<ServerPtr>, std::greater<ServerPtr>> approximate_servers;
        std::deque<RequestPtr> known_requests;
        std::deque<RequestPtr> unknown_requests;
        std::unordered_map<std::string, ServerPtr> processing;
        std::unordered_map<std::string, RequestPtr> requests;
        
        RequestPtr get_smallest_job();
};

LoadBalancer::LoadBalancer(std::vector<std::string> servernames) {
    for (auto iter = servernames.begin(); iter != servernames.end(); iter++) {
        ServerPtr server = std::make_shared<ServerStatistic>(*iter);
        this->approximate_servers.push(server);
    }
    this->last_sent = KNOWN_SIZE;
}

void LoadBalancer::handle_completion(std::string filename) {
    ServerPtr server = this->processing[filename];
    RequestPtr request = this->requests[filename];
    server->record_request(request->complete());
    if (server->is_calibrated()) {
        this->calibrated_servers.push(server);
    } else {
        this->approximate_servers.push(server);
    }
}

void LoadBalancer::handle_request(std::string request_string) {
    logger.write_info(request_string, "Received");
    std::string request_name = parser_filename(request_string);
    int request_size = parser_jobsize(request_string);
    RequestPtr request = std::make_shared<Request>(request_name, request_size);
    this->requests[request_name] = request;
    if (request->get_size() > 0) {
        this->known_requests.push_back(request);
    } else {
        this->unknown_requests.push_back(request);
    }
}

RequestPtr LoadBalancer::get_smallest_job() {
    std::deque<RequestPtr>::iterator current = this->known_requests.begin();
    int size = (*current)->get_size();
    for (std::deque<RequestPtr>::iterator iter = ++this->known_requests.begin(); iter != this->known_requests.end(); ++iter) {
        if ((*iter)->get_size() < size) {
            size = (*iter)->get_size();
            current = iter;
        }
    }
    RequestPtr job = *current;
    this->known_requests.erase(current);
    return job;
}

std::string LoadBalancer::handle_next() {
    auto compare = [](RequestPtr lhs, RequestPtr rhs) {
        if (std::abs(lhs->get_arrival_time() - rhs->get_arrival_time()) < 5E5) {
            return lhs->get_arrival_time() < rhs->get_arrival_time();
        }
        if (lhs->get_size() == -1 || rhs->get_size() == -1) {
            return lhs->get_token() == rhs->get_token();
        }
        return lhs->get_size() > rhs->get_size();
    };
    if (this->calibrated_servers.size() + this->approximate_servers.size() == 0) {
        return "";
    }
    if (this->known_requests.size() + this->unknown_requests.size() == 0) {
        return "";
    }
    ServerPtr server_to_send = nullptr;
    RequestPtr request_to_send = nullptr;
    if (this->approximate_servers.size() > 0 && this->known_requests.size() > 0) {
        server_to_send = this->approximate_servers.top();
        this->approximate_servers.pop();
        logger.write_debug("Allocating job to approximated servers.");
        request_to_send = this->get_smallest_job();
        this->processing[request_to_send->get_name()] = server_to_send;
        this->last_sent = KNOWN_SIZE;
        logger.write_debug("Dispatching job from known queue.");
        std::string scheduled_request = scheduleJobToServer(server_to_send, request_to_send);
        return scheduled_request;
    }
    int request_count = this->unknown_requests.size() + this->known_requests.size();
    int server_count = this->approximate_servers.size() + this->calibrated_servers.size();
    int schedule_count = std::min(request_count, server_count);
    std::string scheduled_request = "";
    std::priority_queue<RequestPtr, std::vector<RequestPtr>,decltype(compare)> queue{compare};
    for (int i = 0; i < schedule_count; ++i) {
        if (this->known_requests.size() == 0) {
            queue.push(this->unknown_requests.front());
            this->unknown_requests.pop_front();
        } else if (this->unknown_requests.size() == 0) {
            queue.push(this->known_requests.front());
            this->known_requests.pop_front();
        } else {
            if (this->known_requests.front()->get_arrival_time() <= this->unknown_requests.front()->get_arrival_time()) {
                queue.push(this->known_requests.front());
                this->known_requests.pop_front();
            } else {
                queue.push(this->unknown_requests.front());
                this->unknown_requests.pop_front();
            }
        }
    }
    for (int i = 0; i < schedule_count; ++i) {
        request_to_send = queue.top();
        queue.pop();
        if (this->approximate_servers.size() == 0) {
            server_to_send = this->calibrated_servers.top();
            this->calibrated_servers.pop();
            logger.write_debug("Allocating job to calibrated servers.");
        } else if (this->calibrated_servers.size() == 0) {
            server_to_send = this->approximate_servers.top();
            this->approximate_servers.pop();
            logger.write_debug("Allocating job to approximated servers.");
        } else {
            if (this->calibrated_servers.top()->get_performance_metric() <= this->approximate_servers.top()->get_performance_metric()) {
                server_to_send = this->calibrated_servers.top();
                this->calibrated_servers.pop();
                logger.write_debug("Allocating job to calibrated servers.");
            } else {
                server_to_send = this->approximate_servers.top();
                this->approximate_servers.pop();
                logger.write_debug("Allocating job to approximated servers.");
            }
        }
        this->processing[request_to_send->get_name()] = server_to_send;
        scheduled_request = scheduled_request + scheduleJobToServer(server_to_send, request_to_send);
    }
    return scheduled_request;
}

// KeyboardInterrupt handler
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    std::cout << "Average scheduling time: " + std::to_string(total_scheduling_time / scheduling_count) << "ms" <<  std::endl;
    std::cout << "Scheduling count: " + std::to_string(scheduling_count) << std::endl;
    std::exit(signum);
}

// send trigger to printAll at servers
void sendPrintAll(const int& serverSocket) {
    std::string message = "printAll\n";
    send(serverSocket, message.c_str(), strlen(message.c_str()), 0);
}

// For example, "1\n2\n3\n4\n5\n" -> "1","2","3","4","5"
// Be careful that "1\n2\n3" -> "1,2" without 3.
std::vector<std::string> parseWithDelimiter(std::string target, std::string delimiter) {
    std::string element;
    std::vector<std::string> ret;
    size_t pos = 0;
    while ((pos = target.find(delimiter)) != std::string::npos) {
        element = target.substr(0, pos);
        ret.push_back(element);
        target.erase(0, pos + delimiter.length());
    }
    return ret;
}

// Parse available severnames
std::vector<std::string> parseServernames(char* buffer, int len) {
    std::string servernames(buffer);
    logger.write_info("Servernames: " + servernames);

    // parse with delimiter ","
    std::vector<std::string> ret = parseWithDelimiter(servernames, ",");
    return ret;
}

std::string parser_filename(std::string request) {
    std::vector<std::string> parsed = parseWithDelimiter(request + ",", ",");
    std::string filename = parsed[0];
    return filename;
}

// parser of request to 2-tuple
int parser_jobsize(std::string request) {
    std::vector<std::string> parsed = parseWithDelimiter(request + ",", ",");
    int jobsize = std::stoi(parsed[1]);
    return jobsize; // it can be -1 (i.e., unknown)
}

// formatting: to assign server to the request
std::string scheduleJobToServer(ServerPtr server, RequestPtr request) {
    std::string schedule = server->get_name() + "," + request->get_name() + "," + std::to_string(request->get_size());
    logger.write_info(schedule + "," + std::to_string(request->get_arrival_time()), "Schedule");
    return schedule + std::string("\n");
}

void parseThenSendRequest(char* buffer, int len, const int& serverSocket, LoadBalancer *load_balancer) {
    // print received requests
    auto start = std::chrono::high_resolution_clock::now();
    std::string sendToServers;

    // parsing to "filename, jobsize" pairs
    std::vector<std::string> request_pairs = parseWithDelimiter(std::string(buffer), "\n");
    int event_count = request_pairs.size();
    for (const auto& request : request_pairs) {
        if (request.find("F") != std::string::npos) {
            std::string completed_filename = std::regex_replace(request, std::regex("F"), "");
            load_balancer->handle_completion(completed_filename);
        } else {
            load_balancer->handle_request(request);
        }
    }
    for (int i = 0; i < event_count; i++) {
        sendToServers = sendToServers + load_balancer->handle_next();
    }
    auto end = std::chrono::high_resolution_clock::now();
    total_scheduling_time += std::chrono::duration<double, std::milli>(end - start).count();
    scheduling_count++;
    if (sendToServers.size() > 0) {
        send(serverSocket, sendToServers.c_str(), strlen(sendToServers.c_str()), 0);
    }
}

int main(int argc, char const* argv[]) {
    signal(SIGINT, signalHandler);
    logger.set_logging_level(DEBUG);
    if (argc != 2) {
        throw std::invalid_argument("must type port number");
        return -1;
    }
    uint32_t portNumber = std::stoi(std::string(argv[1]));

    int serverSocket = 0;
    struct sockaddr_in serv_addr;
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error !");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portNumber);
    // Converting IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address ! This IP Address is not supported !\n");
        return -1;
    }
    if (connect(serverSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed : Can't establish a connection over this socket !");
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;

    // set timeout
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
        printf("setsockopt failed\n");
        return -1;
    }
    if (setsockopt(serverSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0) {
        printf("setsockopt failed\n");
        return -1;
    }

    char buffer[4096] = {0};
    int len;
    logger.write_info("Processing server names");
    len = read(serverSocket, buffer, 4096);
    std::vector<std::string> servernames = parseServernames(buffer, len);
    LoadBalancer load_balancer = LoadBalancer(servernames);
    int currSeconds = -1;
    auto now = std::chrono::system_clock::now();
    while (true) {
        try {
            len = read(serverSocket, buffer, 4096);
            if (len > 0) {
                parseThenSendRequest(buffer, len, serverSocket, &load_balancer);
                memset(buffer, 0, 4096);
            }
            sleep(0.00001);  // sufficient for 50ms granualrity
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }

        // // Example printAll API : let servers print status in every seconds
        // if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - now).count() > currSeconds) {
        //     currSeconds = currSeconds + 1;
        //     sendPrintAll(serverSocket);
        // }
    }
    return 0;
}