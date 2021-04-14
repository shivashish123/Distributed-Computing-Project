#include<bits/stdc++.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#define BUFSIZE 1024
#define SERVERIP "127.0.0.1"
#define pb push_back
using namespace std;
map <pair<int,int>,int> clientPortMap,clientServerSocket;
std::default_random_engine eng;
ofstream output,output2; 
int serverPortSeed,clientPortSeed,m,n;
double lDrift,lWkDrift, lP,lQ,lSend;
int waiting = 0;
int messageCounter=0,listners=0;
// mutex locks for mutual exclusion of shared variables
mutex waitingSetLock,portmapLock,clientServerSocketLock,clientPortMapLock,serverSocketFdsLock,listenerLock,fileLock;
vector <int> serverSocketfds;

/**
 * Helper Class for get the formatted time in HH:MM:SS 
 * */
class Helper {
    private:
    static const int64_t bigConstant = 1600000000000000000; 
    public:

    // gives formatted time in HH::MM::SS
    static string get_formatted_time(time_t t1) 
    {
        struct tm* t2=localtime(&t1);
        char buffer[20];
        sprintf(buffer,"%d : %d : %d",t2->tm_hour,t2->tm_min,t2->tm_sec);
        return buffer;
    }

    // get random number in range (a,b)
    static int64_t getRandomNumber(int64_t a, int64_t b) 
    {    
        if(a>b)
            swap(a,b);
        int64_t out = a + rand() % (b - a + 1);
        return out;
    }
  
};


/**
 * Node class to simulate different nodes in a distributed system
 * Member Variables
 * id                    - Id of server (- n) 
 * serverSocket          - socket descriptor of server on which server is listening for messages
 * serverPort            - portNo of server
 * inDeg,outDeg          - indegree and outdegree of node in toplogy
 * clientListenerThreads - All the listener threads
 * messageSenderThreads  - All the sender threads to setup connection parallely
 * inDegreeVertices      - Incomming vertices in graph topology
 * outDegreeVertices     - Outgoing vertices in graph topology
 * server                - Thread to setup server port and listen for messages
 * senderThread          - Thread to send messages to different nodes
 * totalSent             - total Messages sent
 * int64_t t1            - storing time of message request sent in nanoseconds 
 *                       - (will be overwritten on every request)     
 * port_idx              - Map to store clientSockets for a correponding client port 
 * nodeLocalClock        - Local clock to read time in nanoseconds
 * exponential_lP        - Exponential number generator
 * exponential_lQ        - Exponential number generator
 * exponential_lSend     - Exponential number generator   
 * waitingForResponse    - Binary Semaphore to wait for message request to be fulfiled
 *                       - before sending other request
 * 
 * Member Methods 
 * 
 * initServerNode()      
 *  - Initializes server port and is part of server thread's funciton
 *    It accepts all the connections and stores all the connected sockets     
 *    in port_idx map
 * 
 * listenForMessage(clientId):
 *  - listen for a message comming from a particular client id
 * 
 * setUpConnectionPort(serverPort , serverId)
 *  - Set up socket file descriptor for a connection between current node 
 *  - and a server with id serverId, for sending message to that server   
 *    
 * sendMessage()          
 *  - send syncronization request to server k times selected randomly
 * 
 * sendMessageToSocket(int recieverSocket,string message)
 *  - Sends a string message to a given socket  
 * 
 * setUpConnectionPorts()
 *  - calls initConnection ports which start message sender threads
 *  - with sendMessage as execution function 
 * 
 * startListenerThreads()
 *  - calls initClientListnerThreads which 
 *  - starts listener Threads with listenMessage as execution function
 * 
 * sendMessageThread() *  - 
 *  - start a thread with sendMessage as execution sendMessage function 
 * 
 * parseString()
 *  - Parses message and breaks them into individiual messages 
 *  - each enclosed in  square brackets []
 * 
 * parseQueryString()
 *  - Parses message which are exchanged between servers
 *  - If starting character is q then it is query message asking for t2,t3
 *      "[q+(totalSent)+*+(id)+]";
 *  - If starting character is r then it is a query response message
 *      "[r+(t2)+*+(t3)+]"; 
 * 
 */
class Node{

    int id;
    int portNo;
    int serverSocket;
	int serverPort;
	int clientCounter = 1;
    int inDeg,outDeg;
	thread* clientListenerThreads;
	thread* messageSenderThreads;
    vector<int> inDegreeVertices ,outDegreeVertices;
    int* clientSocketIds ;
    thread server,senderThread; 
    sem_t waitingForResponse;
    int totalSent = 1;
    int64_t t1;
    map <int,int> port_idx;
    std::exponential_distribution<double>exponential_lP;
    std::exponential_distribution<double>exponential_lQ;
    std::exponential_distribution<double>exponential_lSend;
   
    public:
    Node(vector<int> neighBourVertices,int id){
        cout<<id<<" :: ";
        for(auto k:neighBourVertices)
            cout<<k<<" ";
        cout<<endl;
        this->inDegreeVertices  = neighBourVertices; // indegree vertices in graph
        this->outDegreeVertices = neighBourVertices; // outdegree vertices in graph
        inDeg = inDegreeVertices.size();
        outDeg = outDegreeVertices.size();
        clientSocketIds = new int[n + 1];  // client sockets
        clientListenerThreads = new thread[n + 1]; // threads memory allocation
        messageSenderThreads  = new thread[n + 1];
        this->id = id;   
        sem_init(&waitingForResponse, 0, 0); // initialize semaphore
        init();
    }
    void startListenerThreads(){ // server setup completed create listner threads
        server.join();
        initClientListnerThreads();
    }
    void setUpConnectionPorts(){ // setup conneciton ports for different recievers (outdegree)
        initConnectionPorts();
    }
    void sendMessageThread(){ // start message sender thread
        senderThread = thread(&Node::sendMessage,this);          
    }
    void SendMessageThreadJoin(){
        cout<<"joined"<<endl;
        senderThread.join();
    }
    ~Node(){ // Destructor        

        for(int i=0;i<inDeg;i++)
            clientListenerThreads[i].join();

        for(int i=0;i<outDeg;i++)
            messageSenderThreads[i].join();
    }


    private:
        void initServerNode(){ 

            in_port_t servPort = serverPortSeed + id; // Local port

            // create socket for incoming connections           
            if ((serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                perror("socket() failed");
                exit(-1);
            }
            
            int optval = 1;
            setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &optval, sizeof(optval));

            // Set local parameters
            struct sockaddr_in servAddr;
            memset(&servAddr, 0, sizeof(servAddr));
            servAddr.sin_family = AF_INET;
            servAddr.sin_addr.s_addr = htons(INADDR_ANY);
            servAddr.sin_port = htons(servPort);

            // Bind to the local address
            if (bind(serverSocket, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
                perror("bind() failed");
                exit(-1);
            }
            // Listen to the client
            if (listen(serverSocket, (inDeg)) < 0) {
                perror("listen() failed");
                exit(-1);
            }
           
			// initialize clientSocket Id's 
			
			struct sockaddr_in clntAddr;
			socklen_t clntAddrLen = sizeof(clntAddr);

            waitingSetLock.lock();
            // Add this id to the waiting set as the server is now in listening state
            waiting--; 
            waitingSetLock.unlock();

            // Clients not necessarily connect in usual order so 
            // we need to do propper maping of clients port to clientSocketid
            for(int clientCounter=0;clientCounter<(inDeg);clientCounter++){ 
                // clientSocketIds[clientCounter] will store the socket file discripter
                // for this connection
                clientSocketIds[clientCounter] = accept(serverSocket, (struct sockaddr *) &clntAddr, &clntAddrLen);
				if (clientSocketIds[clientCounter] < 0) {
					perror("accept() failed");
					exit(-1);
				}
                
                // now we need to which client connected 
                // port_idx map will store the socket file descriptor for the connection between
                // server id and clinet with port clntAddr.sin_port
                // While listening for a message we wil require this to get the corresponding
                // socket where the server can listen
                char clntIpAddr[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr,clntIpAddr, sizeof(clntIpAddr)) != NULL) {
                    //printf("----\nHandling client %s %d for %d\n",
                    //clntIpAddr, clntAddr.sin_port,id);
                    portmapLock.lock();
                    port_idx[clntAddr.sin_port] = clientSocketIds[clientCounter];
                    portmapLock.unlock();
                } else {
                    puts("----\nUnable to get client IP Address");
                }              
			}   
        }
		void setUpConnectionPort(int serverPort , int serverId){
			//Creat a socket
			int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sockfd < 0) {
				perror("socket() failed");
				exit(-1);
			}	
			
			int optval = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

			
			// Set the server address servAddr will store details of server
			struct sockaddr_in servAddr , myOwnAddr;

			memset(&servAddr, 0, sizeof(servAddr));           
			servAddr.sin_family = AF_INET;
			int err = inet_pton(AF_INET, SERVERIP, &servAddr.sin_addr.s_addr);
			if (err <= 0) {
				perror("inet_pton() failed");
				exit(-1);
			}
			servAddr.sin_port = htons(serverPort);

            // Also create sockaddr_in struct for the client thread whihc will connect to server
            // myOwnAddr will store the details of the client thread
            memset(&myOwnAddr, 0, sizeof(myOwnAddr));
            myOwnAddr.sin_family = AF_INET;
			int err2 = inet_pton(AF_INET, SERVERIP, &myOwnAddr.sin_addr.s_addr);
			if (err2 <= 0) {
				perror("inet_pton() failed");
				exit(-1);
			}
            // clients port clientPortSeed+id+100*serverId to uniquely define each port
			myOwnAddr.sin_port = htons(clientPortSeed+id+1000*serverId);
            if (bind(sockfd, (struct sockaddr *) &myOwnAddr, sizeof(myOwnAddr)) < 0) {
                perror("bind() failed");
                exit(-1);
            }

			// Connect to server
			if (connect(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
				perror("connect() failed");
				exit(-1);
			}

            // while sending message from id to serverId this clients port was used
            // It will be used by server "serverId" to get the socket file descriptor where it will listen
            // for messages from this client "id"

            // Commented out
            clientPortMapLock.lock();
            usleep(1000);
            clientPortMap[{serverId,id}] = myOwnAddr.sin_port ; 
            usleep(1000);
            clientPortMapLock.unlock();


            // records clients socket , serverid and id will uniquely define the socket file descriptor
            // while sending message to server serverId, client id will use this socket.

            // Commented out
            clientServerSocketLock.lock();
            usleep(1000);
            clientServerSocket[{serverId,id}] = sockfd;
            usleep(1000);
            clientServerSocketLock.unlock();
        }

		void listenForMessage(int clientId){
            // buffer will store the message 
            char buffer[BUFSIZE];
            memset(buffer, 0, BUFSIZE); // reset the buffer
            ssize_t recvLen ;     
            int socketToListen;
            // clientPortMap will give the client's port for the connection between 
            // server id and client with id clientId
            clientPortMapLock.lock();
            int clientPortId = clientPortMap[{id,clientId}]; 
            clientPortMapLock.unlock();
            
            // We need clientPort as it is unique for every connection with different server 
            
            // port_idx will give the socket file descriptor for connection between server id
            // and client with clientport clientPortId

            while((socketToListen = port_idx[clientPortId]) == 0 );
            serverSocketFdsLock.lock();
            usleep(1000);
                serverSocketfds.push_back(socketToListen);
            usleep(1000);
            serverSocketFdsLock.unlock();
            listenerLock.lock();
            listners--;
            listenerLock.unlock();
            while( recvLen =  recv(socketToListen, buffer, BUFSIZE - 1, 0) > 0){
                string message = string(buffer);
                vector <string> sendersStrings = parseString(message);
                for(auto senderString : sendersStrings){

                    time_t RecvTime=time(NULL); // build string to log to file
                    string formatted_time=Helper::get_formatted_time(RecvTime);
                    // senderId = id of node to which we have to send message
                    // int recieverSocket = clientServerSocket[{senderId,id}];
                    // clientServerSocketLock.unlock();

                    // ssize_t sentLen = sendMessageToSocket(recieverSocket,responseString);              

                  
                    memset(buffer, 0, BUFSIZE); // reset buffer
                }
            }

        }
        int sendMessageToSocket(int recieverSocket,string message){
            int serverSleepTime = exponential_lSend(eng);
            usleep(serverSleepTime*100);
            ssize_t sentLen = send(recieverSocket,message.c_str(), strlen(message.c_str()), 0);
            return sentLen;
        }
        void sendMessage(){
         
                cout<<"Send message"<<endl;
                // int reciever = outDegreeVertices[randomOutDegreeIndex] ;
                // clientServerSocketLock.lock();
                // int recieverSocket = clientServerSocket[{reciever,id}];
                // clientServerSocketLock.unlock();
                // string message = "some message"
                // ssize_t sentLen = sendMessageToSocket(recieverSocket,message);                                 
		}

      
        // Used by listener thread to parse the incomming string
        // and get the x,vt[x] pairs using which vector time of
        // the listner process will be updated
        vector< string> parseString(string str){
            // vector< string> senderStrings;        
            // return senderStrings;
        }       

        // creates listner thread total no is given by indegree of this node in the graph
        void initClientListnerThreads(){
            for(int i=0;i<inDeg;i++)
                {
					clientListenerThreads[i] = thread(&Node::listenForMessage,this,inDegreeVertices[i]);
                }
        }

        // We initialize connection ports for the message sender threads
        // total no given by outdegree of node in graph
		void initConnectionPorts(){
			for(int i=0;i<(outDeg);i++){
    			messageSenderThreads[i] = thread(&Node::setUpConnectionPort,this , serverPortSeed+outDegreeVertices[i] , outDegreeVertices[i]);
            }
            for(int i=0;i<outDeg;i++){
                messageSenderThreads[i].join();
            }
		}

        // we initialize the server's port
        void init(){   
            server = thread(&Node::initServerNode,this);            
        }
};

int main()
{

    ifstream input("inp-params.txt"); // take input from inp-params.txt
    output.open("Log.txt");
    output2.open("Log2.txt");
    string str2;        
    input>>n>>m;
    eng.seed(4);

    vector <int> inverseAdjacencyList[n+5]; // to keep track of nodes that will send message to me
    vector <int> adjacencyList[n+5]; // to keep track of nodes whom I will send messages
    Node* nodes[n+5]; // Create n nodes


    waiting= n;
    // Input Handling
    // Create random serverPortSeed and ClientPortSeed 
    // Using this as base seed client and server threads will compute their port numbers

    srand(time(NULL));
    serverPortSeed = Helper::getRandomNumber(20000,40000);
    clientPortSeed = Helper::getRandomNumber(40000 ,60000);
    int totalIndeg = 0;
    for(int i=1;i<=m;i++){  
        int u,v;
        input>>u>>v;
        adjacencyList[u].pb(v);
        adjacencyList[v].pb(u);       
    }

    for(int i=1;i<=n;i++){
        nodes[i] = new Node(adjacencyList[i], i); // create a node 
        listners+= adjacencyList[i].size() ;
    }
   
   
    while(waiting>0); // Wait till the constructor has finished and server nodes are setup
    for(int i=1;i<=n;i++){
        nodes[i]->setUpConnectionPorts();
    }
    cout<<"connection ports setup\n";
    for(int i=1;i<=n;i++){
        nodes[i]->startListenerThreads();
    }  
    while(listners > 0);



    cout<<"setup completed"<<endl;
    // initiator thread 
    nodes[1]->sendMessageThread();
    
    nodes[1]->SendMessageThreadJoin();
    
}