#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <bitset>
#include <chrono>
#include <csignal>
#include <deque>
#include <iostream>
#include <memory>
#include <queue>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define IDENTIFIED_REQUEST 1
#define UNIDENTIFIED_REQUEST 0
#define DEFAULT_MULTIPLIER 2
#define HALF_SECOND 500

class Request : public std::enable_shared_from_this<Request> {
    public:
        Request(std::string, int);
        std::string get_name();
        std::shared_ptr<Request> start();
        std::shared_ptr<Request> complete();
        int get_size();
        void set_forced();
        bool check_forced();
        int get_arrival_time();
        int get_service_time();
    private:
        std::string name;
        int size;
        bool forced;
        std::chrono::system_clock::time_point arrival_time;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point completion_time;
};

Request::Request(std::string request_name, int request_size) {
    this->arrival_time = std::chrono::system_clock::now();
    this->name = request_name;
    this->size = request_size;
    this->forced = false;
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

void Request::set_forced() {
    this->forced = true;
}

bool Request::check_forced() {
    return this->forced;
}

int Request::get_arrival_time() {
    if (this->arrival_time == std::chrono::system_clock::time_point()) {
        return -1;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(this->arrival_time.time_since_epoch()).count();
}

int Request::get_service_time() {
    if (this->start_time == std::chrono::system_clock::time_point() || this->completion_time == std::chrono::system_clock::time_point()) {
        return -1;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(this->completion_time - this->start_time).count();
}

bool operator<(RequestPtr lhs, RequestPtr rhs) {
    return lhs->get_arrival_time() < rhs->get_arrival_time();
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
        void process_request(RequestPtr);
        bool record_request(RequestPtr);
        std::string get_name();
        int active_request_count();
        double get_response_time();
        double get_performance_metric();
        bool is_calibrated();
    private:
        std::string name;
        int requests;
        int requests_completed;
        Average performance_metric;
        Average response_time;
};

using ServerPtr = std::shared_ptr<ServerStatistic>;

ServerStatistic::ServerStatistic(std::string server_name) {
    this->name = server_name;
    this->requests = 0;
    this->requests_completed = 0;
    this->performance_metric = Average();
    this->response_time = Average();
}

std::string ServerStatistic::get_name() {
    return this->name;
}

void ServerStatistic::process_request(RequestPtr request) {
    request->start();
    this->requests++;
}

bool ServerStatistic::record_request(RequestPtr request) {
    request->complete();
    this->requests_completed++;
    int request_size = request->get_size();
    int service_time = request->get_service_time();
    this->response_time.record(service_time);
    if (this->requests != this->requests_completed) {
        return false;
    }
    if (this->requests == 1) {
        if (request_size > 0) {
            this->performance_metric.record(service_time / request_size);
        }
    }
    this->requests = 0;
    this->requests_completed = 0;
    return true;
}

int ServerStatistic::active_request_count() {
    return this->requests - this->requests_completed;
}

double ServerStatistic::get_response_time() {
    return this->response_time.query();
}

double ServerStatistic::get_performance_metric() {
    return this->performance_metric.query();
}

bool ServerStatistic::is_calibrated() {
    return this->performance_metric.is_valid();
}

bool operator<(ServerPtr lhs, ServerPtr rhs) {
    double lhs_performance_metric = lhs->get_performance_metric();
    double rhs_performance_metric = rhs->get_performance_metric();
    if (lhs_performance_metric == -1 || rhs_performance_metric == -1) {
        double lhs_response_time = lhs->get_response_time();
        double rhs_response_time = rhs->get_response_time();
        return lhs_response_time < rhs_response_time;
    }
    return lhs_performance_metric < rhs_performance_metric;
}

// Function declarations
std::vector<std::string> parse_with_delimiter(std::string, std::string);
std::vector<std::string> parse_server_names(char*, int);
std::string parser_filename(std::string);
int parse_request_size(std::string);
std::string schedule_request_to_server(ServerPtr, RequestPtr);

class LoadBalancer {
    public:
        LoadBalancer(std::vector<std::string>);
        void handle_completion(std::string);
        void handle_request(std::string);
        std::string handle_next();
        std::string handle_timeout();
    private:
        std::vector<ServerPtr> servers;
        std::priority_queue<ServerPtr, std::vector<ServerPtr>, std::greater<ServerPtr>> calibrated_servers;
        std::priority_queue<ServerPtr, std::vector<ServerPtr>, std::greater<ServerPtr>> approximated_servers;
        std::deque<RequestPtr> identified_requests;
        std::deque<RequestPtr> unidentified_requests;
        std::unordered_map<std::string, ServerPtr> processing;
        std::unordered_map<std::string, RequestPtr> requests;
        int multiplier;
        int active_forced_requests;
        int active_forced_requests_completed;
        std::chrono::system_clock::time_point timeout_trigger;
        
        void reset_timeout();
        double average_response_time();
        RequestPtr get_smallest_request();
        ServerPtr get_timeout_handler();
};

LoadBalancer::LoadBalancer(std::vector<std::string> servernames) {
    for (auto iter = servernames.begin(); iter != servernames.end(); ++iter) {
        ServerPtr server = std::make_shared<ServerStatistic>(*iter);
        this->approximated_servers.push(server);
        this->servers.push_back(server);
    }
    this->active_forced_requests = 0;
    this->active_forced_requests_completed = 0;
    this->reset_timeout();
}

void LoadBalancer::handle_completion(std::string filename) {
    ServerPtr server = this->processing[filename];
    RequestPtr request = this->requests[filename];
    bool ready = server->record_request(request);
    if (request->check_forced()) {
        this->active_forced_requests_completed++;
        if (this->active_forced_requests_completed == this->active_forced_requests) {
            this->active_forced_requests = 0;
            this->active_forced_requests_completed = 0;
            this->reset_timeout();
        }
    }
    if (!ready) {
        return;
    }
    if (server->is_calibrated()) {
        this->calibrated_servers.push(server);
    } else {
        this->approximated_servers.push(server);
    }
}

void LoadBalancer::handle_request(std::string request_string) {
    std::string request_name = parser_filename(request_string);
    int request_size = parse_request_size(request_string);
    RequestPtr request = std::make_shared<Request>(request_name, request_size);
    this->requests[request_name] = request;
    if (request->get_size() > 0) {
        this->identified_requests.push_back(request);
    } else {
        this->unidentified_requests.push_back(request);
    }
}

std::string LoadBalancer::handle_next() {
    if (this->calibrated_servers.size() + this->approximated_servers.size() == 0) {
        return "";
    }
    if (this->identified_requests.size() + this->unidentified_requests.size() == 0) {
        return "";
    }
    ServerPtr server_to_send = nullptr;
    RequestPtr request_to_send = nullptr;
    if (this->approximated_servers.size() > 0 && this->identified_requests.size() > 0) {
        server_to_send = this->approximated_servers.top();
        this->approximated_servers.pop();
        request_to_send = this->get_smallest_request();
        this->processing[request_to_send->get_name()] = server_to_send;
        std::string scheduled_request = schedule_request_to_server(server_to_send, request_to_send);
        this->reset_timeout();
        return scheduled_request;
    } 
    if (this->approximated_servers.size() == 0) {
        server_to_send = this->calibrated_servers.top();
        this->calibrated_servers.pop();
    } else if (this->calibrated_servers.size() == 0) {
        server_to_send = this->approximated_servers.top();
        this->approximated_servers.pop();
    } else {
        if (this->calibrated_servers.top()->get_response_time() <= this->approximated_servers.top()->get_response_time()) {
            server_to_send = this->calibrated_servers.top();
            this->calibrated_servers.pop();
        } else {
            server_to_send = this->approximated_servers.top();
            this->approximated_servers.pop();
        }
    }
    if (this->identified_requests.size() == 0) {
        request_to_send = this->unidentified_requests.front();
        this->unidentified_requests.pop_front();
    } else if (this->unidentified_requests.size() == 0) {
        request_to_send = this->identified_requests.front();
        this->identified_requests.pop_front();
    } else {
        if (this->identified_requests.front() < this->unidentified_requests.front()) {
            request_to_send = this->identified_requests.front();
            this->identified_requests.pop_front();
        } else {
            request_to_send = this->unidentified_requests.front();
            this->unidentified_requests.pop_front();
        }
    }
    this->processing[request_to_send->get_name()] = server_to_send;
    std::string scheduled_request = schedule_request_to_server(server_to_send, request_to_send);
    this->reset_timeout();
    return scheduled_request;
}

std::string LoadBalancer::handle_timeout() {
    if (this->unidentified_requests.size() + this->identified_requests.size() == 0) {
        return "";
    }
    std::chrono::system_clock::time_point current = std::chrono::system_clock::now();
    int time_since_last_sent = std::chrono::duration_cast<std::chrono::milliseconds>(current - this->timeout_trigger).count();
    if (time_since_last_sent < this->multiplier * this->average_response_time()) {
        return "";
    }
    int queue;
    if (this->unidentified_requests.size() == 0) {
        queue = IDENTIFIED_REQUEST;
    } else if (this->identified_requests.size() == 0) {
        queue = UNIDENTIFIED_REQUEST;
    } else {
        if (this->identified_requests.front()->get_arrival_time() < this->unidentified_requests.front()->get_arrival_time()) {
            queue = IDENTIFIED_REQUEST;
        } else {
            queue = UNIDENTIFIED_REQUEST;
        }
    }
    RequestPtr request_to_send;
    if (queue == IDENTIFIED_REQUEST) {
        request_to_send = this->identified_requests.front();
    } else {
        request_to_send = this->unidentified_requests.front();
    }
    double current_time = std::chrono::duration_cast<std::chrono::milliseconds>(current.time_since_epoch()).count();
    if (current_time - request_to_send->get_arrival_time() < DEFAULT_MULTIPLIER * this->average_response_time()) {
        return "";
    }
    request_to_send->set_forced();
    if (queue == IDENTIFIED_REQUEST) {
        this->identified_requests.pop_front();
    } else {
        this->unidentified_requests.pop_front();
    }
    ServerPtr server_to_send = this->get_timeout_handler();
    this->processing[request_to_send->get_name()] = server_to_send;
    std::string scheduled_request = schedule_request_to_server(server_to_send, request_to_send);
    this->multiplier <<= 1;
    this->active_forced_requests++;
    return scheduled_request;
}

void LoadBalancer::reset_timeout() {
    this->multiplier = DEFAULT_MULTIPLIER;
    this->timeout_trigger = std::chrono::system_clock::now();
}

double LoadBalancer::average_response_time() {
    int server_count = 0;
    double total_response_time = 0;
    for (std::vector<ServerPtr>::iterator iter = this->servers.begin(); iter != this->servers.end(); ++iter) {
        double response_time = (*iter)->get_response_time();
        if (response_time != -1) {
            server_count++;
            total_response_time += response_time;
        }
    }
    if (server_count == 0) {
        return HALF_SECOND;
    } else {
        return total_response_time / server_count;
    }
}

RequestPtr LoadBalancer::get_smallest_request() {
    std::deque<RequestPtr>::iterator current = this->identified_requests.begin();
    int size = (*current)->get_size();
    for (std::deque<RequestPtr>::iterator iter = ++this->identified_requests.begin(); iter != this->identified_requests.end(); ++iter) {
        if ((*iter)->get_size() < size) {
            size = (*iter)->get_size();
            current = iter;
        }
    }
    RequestPtr job = *current;
    this->identified_requests.erase(current);
    return job;
}

ServerPtr LoadBalancer::get_timeout_handler() {
    std::vector<ServerPtr>::iterator candidate = this->servers.begin();
    for (std::vector<ServerPtr>::iterator iter = ++this->servers.begin(); iter != this->servers.end(); ++iter) {
        int candidate_active_request_count = (*candidate)->active_request_count();
        int check_active_request_count = (*iter)->active_request_count();
        double candidate_response_time = (*candidate)->get_response_time();
        double check_response_time = (*iter)->get_response_time();
        if (candidate_response_time == -1 || check_response_time == -1) {
            if (check_active_request_count < candidate_active_request_count) {
                candidate = iter;
            }
        } else if (candidate_active_request_count * candidate_response_time > check_active_request_count * check_response_time) {
            candidate = iter;
        }
    }
    return *candidate;
}

// KeyboardInterrupt handler
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    std::exit(signum);
}

// send trigger to printAll at servers
void sendPrintAll(const int& serverSocket) {
    std::string message = "printAll\n";
    send(serverSocket, message.c_str(), strlen(message.c_str()), 0);
}

// For example, "1\n2\n3\n4\n5\n" -> "1","2","3","4","5"
// Be careful that "1\n2\n3" -> "1,2" without 3.
std::vector<std::string> parse_with_delimiter(std::string target, std::string delimiter) {
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
std::vector<std::string> parse_server_names(char* buffer, int len) {
    std::string servernames(buffer);

    // parse with delimiter ","
    std::vector<std::string> ret = parse_with_delimiter(servernames, ",");
    return ret;
}

std::string parser_filename(std::string request) {
    std::vector<std::string> parsed = parse_with_delimiter(request + ",", ",");
    std::string filename = parsed[0];
    return filename;
}

// parser of request to 2-tuple
int parse_request_size(std::string request) {
    std::vector<std::string> parsed = parse_with_delimiter(request + ",", ",");
    int request_size = std::stoi(parsed[1]);
    return request_size; // it can be -1 (i.e., unknown)
}

// formatting: to assign server to the request
std::string schedule_request_to_server(ServerPtr server, RequestPtr request) {
    server->process_request(request);
    std::string schedule = server->get_name() + "," + request->get_name() + "," + std::to_string(request->get_size());
    return schedule + std::string("\n");
}

void parse_and_send_request(char* buffer, int len, const int& server_socket, LoadBalancer *load_balancer) {
    // print received requests
    std::string send_to_servers;

    // parsing to "filename, request_size" pairs
    std::vector<std::string> request_pairs = parse_with_delimiter(std::string(buffer), "\n");
    int event_count = request_pairs.size();
    for (const auto& request : request_pairs) {
        if (request.find("F") != std::string::npos) {
            std::string completed_filename = std::regex_replace(request, std::regex("F"), "");
            load_balancer->handle_completion(completed_filename);
        } else {
            load_balancer->handle_request(request);
        }
    }
    for (int i = 0; i < event_count; ++i) {
        send_to_servers = send_to_servers + load_balancer->handle_next();
    }
    if (send_to_servers.size() > 0) {
        send(server_socket, send_to_servers.c_str(), strlen(send_to_servers.c_str()), 0);
    }
}

void handle_timeout(const int& server_socket, LoadBalancer *load_balancer) {
    std::string send_to_servers = load_balancer->handle_timeout();
    if (send_to_servers.size() > 0) {
        send(server_socket, send_to_servers.c_str(), strlen(send_to_servers.c_str()), 0);
    }
}

int main(int argc, char const* argv[]) {
    signal(SIGINT, signalHandler);
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
    len = read(serverSocket, buffer, 4096);
    std::vector<std::string> servernames = parse_server_names(buffer, len);
    LoadBalancer load_balancer = LoadBalancer(servernames);
    int cycles = 0;
    auto now = std::chrono::system_clock::now();
    while (true) {
        try {
            len = read(serverSocket, buffer, 4096);
            if (len > 0) {
                parse_and_send_request(buffer, len, serverSocket, &load_balancer);
                memset(buffer, 0, 4096);
            }
            if (cycles == 10) {
                handle_timeout(serverSocket, &load_balancer);
                cycles = -1;
            }
            ++cycles;
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