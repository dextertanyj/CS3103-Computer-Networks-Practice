from datetime import datetime
import socket
import sys
import argparse
import signal

servers = dict()
fileToServer = dict()
queue = []
freeServers = []
initialServers = []


avgPacketSize = 30


class Server:
    def __init__(self, name):
        self.name = name
        self.capacity = -1
        self.waitTime = 0
        self.serveSize = 0

    def updateCapacity(self, capacity):
        self.capacity = capacity

    def removeJob(self, job_size):
        self.serveSize = self.serveSize - job_size
        if self.capacity > 0:
            self.waitTime = self.waitTime - job_size / self.capacity

    def addJob(self, job_size):
        self.serveSize = self.serveSize + job_size
        if self.capacity > 0:
            timeNeeded = job_size / self.capacity
            self.waitTime = self.waitTime + timeNeeded

    def getWaitTime(self, job_size):
        if self.capacity > 0:
            return self.waitTime + (job_size / self.capacity)
        else:
            return 0


class Job:
    def __init__(self, server, job_size, unknown_size):
        self.server = server
        self.job_size = job_size
        self.start = datetime.now()
        self.unknown_size = unknown_size


# KeyboardInterrupt handler
def sigint_handler(signal, frame):
    print('KeyboardInterrupt is caught. Close all sockets :)')
    sys.exit(0)


# send trigger to printAll at servers
def sendPrintAll(serverSocket):
    serverSocket.send(b"printAll\n")


# Parse available severnames
def parseServernames(binaryServernames):
    return binaryServernames.decode().split(',')[:-1]


# get the completed file's name, what you want to do?
def getCompletedFilename(filename):
    ####################################################
    #                      TODO                        #
    # You should use the information on the completed  #
    # job to update some statistics to drive your      #
    # scheduling policy. For example, check timestamp, #
    # or track the number of concurrent files for each #
    # server?                                          #
    ####################################################

    job = fileToServer.get(filename)
    server = servers.get(job.server)
    if not job.unknown_size:
        if server.capacity == -1:
            end = datetime.now()
            timeTaken = (end - job.start).total_seconds()
            capacity = job.job_size / timeTaken
            server.updateCapacity(int(capacity))

    server.removeJob(job.job_size)
    fileToServer.pop(filename)
    if server.serveSize == 0:
        freeServers.append(server.name)
        sendJobsToServer()

    # In this example. just print message
    print(f"[JobScheduler] Filename {filename} is finished.")


# formatting: to assign server to the request
def scheduleJobToServer(servername, request):
    return (servername + "," + request + "\n").encode()


def sendJobsToServer():
    while queue:
        if freeServers:
            request = queue.pop(0)
            request_name = request.split(",")[0]
            request_size = avgPacketSize if (int(request.split(",")[1]) == -1) else int(request.split(",")[1])
            unknown_size = True if (int(request.split(",")[1]) == -1) else False
            server_to_send = ''
            min_wait = sys.maxsize

            for key in freeServers:
                server = servers.get(key)
                if server.capacity < 0:
                    server_to_send = server.name
                    break
                elif server.getWaitTime(request_size) < min_wait:
                    server_to_send = server.name
                    min_wait = server.getWaitTime(request_size)
            freeServers.remove(server_to_send)

            job = Job(server_to_send, request_size, unknown_size)
            servers.get(server_to_send).addJob(request_size)
            fileToServer[request_name] = job
            scheduled_request = scheduleJobToServer(server_to_send, request)
            sendToServers = b"" + scheduled_request
            serverSocket.send(sendToServers)
        else:
            break


# main part you need to do
def assignServerToRequest(servernames, request):
    ####################################################
    #                      TODO                        #
    # Given the list of servers, which server you want #
    # to assign this request? You can make decision.   #
    # You can use a global variables or add more       #
    # arguments.                                       #

    if initialServers:
        request_name = request.split(",")[0]
        request_size = avgPacketSize if (int(request.split(",")[1]) == -1) else int(request.split(",")[1])
        unknown_size = True if (int(request.split(",")[1]) == -1) else False
        server_to_send = initialServers.pop(0)
        job = Job(server_to_send, request_size, unknown_size)
        servers.get(server_to_send).addJob(request_size)
        fileToServer[request_name] = job
        # Schedule the job
        scheduled_request = scheduleJobToServer(server_to_send, request)
        return scheduled_request
    else:
        queue.append(request)
        return b""


def parseThenSendRequest(clientData, serverSocket, servernames):
    # print received requests
    print(f"[JobScheduler] Received binary messages:\n{clientData}")
    print(f"--------------------")
    # parsing to "filename, jobsize" pairs
    requests = clientData.decode().split("\n")[:-1]

    sendToServers = b""
    for request in requests:
        if request[0] == "F":
            # if completed filenames, get the message with leading alphabet "F"
            filename = request.replace("F", "")
            getCompletedFilename(filename)
        else:
            # if requests, add "servername" front of the pairs -> "servername, filename, jobsize"
            sendToServers = sendToServers + \
                            assignServerToRequest(servernames, request)

            # send "servername, filename, jobsize" pairs to servers
    if sendToServers != b"":
        serverSocket.send(sendToServers)


if __name__ == "__main__":
    # catch the KeyboardInterrupt error in Python
    signal.signal(signal.SIGINT, sigint_handler)

    # parse arguments and get port number
    parser = argparse.ArgumentParser(description="JobScheduler.")
    parser.add_argument('-port', '--server_port', action='store', type=str, required=True,
                        help='port to server/client')
    args = parser.parse_args()
    server_port = int(args.server_port)

    # open socket to servers
    serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serverSocket.connect(('127.0.0.1', server_port))

    # IMPORTANT: for 50ms granularity of emulator
    serverSocket.settimeout(0.0001)

    # receive preliminary information: servernames (can infer the number of servers)
    binaryServernames = serverSocket.recv(4096)
    servernames = parseServernames(binaryServernames)
    print(f"Servernames: {servernames}")

    for i in servernames:
        servers[i] = Server(i)
        initialServers.append(i)

    currSeconds = -1
    now = datetime.now()
    while (True):
        try:
            # receive the completed filenames from server
            completeFilenames = serverSocket.recv(4096)
            if completeFilenames != b"":
                parseThenSendRequest(
                    completeFilenames, serverSocket, servernames)
        except socket.timeout:
            # IMPORTANT: catch timeout exception, DO NOT REMOVE
            pass

        # # Example printAll API : let servers print status in every seconds
        # if (datetime.now() - now).seconds > currSeconds:
        #     currSeconds = currSeconds + 1
        #     sendPrintAll(serverSocket)
