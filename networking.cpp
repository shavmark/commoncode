#include "ofApp.h"
#include "control.h"

namespace Software2552 {
#define MAXSEND (1024*1024)

	shared_ptr<ofxJSON> OSCMessage::toJson(shared_ptr<ofxOscMessage> m) {
		if (m) {
			// bugbug data comes from one of http/s, string or local file but for now just a string
			shared_ptr<ofxJSON> p = std::make_shared<ofxJSON>();
			if (p) {
				string output;
				string input = m->getArgAsString(0);
				if (uncompress(input.c_str(), input.size(), output)) {
					p->parse(output);
				}
			}
			return p;
		}
		return nullptr;
	}
	shared_ptr<ofxOscMessage> OSCMessage::fromJson(ofxJSON &data, const string&address) {
		shared_ptr<ofxOscMessage> p = std::make_shared<ofxOscMessage>();
		if (p) {
			p->setAddress(address); // use 32 bits so we can talk to everyone easiy
									//bugbug if these are used find a way to parameterize
									//bugbug put all these items in json? or instead use them
									// to ignore messages, delete old ones?
			// even compress the small ones so more messages can use UDP
			string output;
			string input = data.getRawString(false);
			if (compress(input.c_str(), input.size(), output)) {
				p->addStringArg(output); // all data is in json
			}
		}
		return p;
	}
	void WriteOsc::setup(const string &hostname, int port) {
		sender.setup(hostname, port);
		startThread();
	}
	void WriteOsc::threadedFunction() {
		while (1) {
			if (q.size() > 0) {
				lock();
				shared_ptr<ofxOscMessage> m = q.front();
				q.pop_front();
				unlock();
				if (m) {
					sender.sendMessage(*m, false);
				}
			}
			ofSleepMillis(10);
		}
	}
	// return true to ignore messages that have been added recently
	bool WriteOsc::ignoreDups(shared_ptr<ofxOscMessage> p, ofxJSON &data, const string&address) {
		// only add if its not in the list already 
		for (int i = 0; i < memory.size(); ++i) {
			if (memory[i]->getAddress() == address && memory[i]->getArgAsString(0) == data.getRawString()) {
				return true; // ignore dup
			}
		}
		memory.push_front(p);
		if (memory.size() > 1000) {
			memory.erase(memory.end() - 200, memory.end());// find to the 1000 and 200 magic numbers bugbug
		}
		return false;
	}
	// add a message to be sent
	void WriteOsc::update(ofxJSON &data, const string&address) {
		if (data.size() > 0) {
			shared_ptr<ofxOscMessage> p = OSCMessage::fromJson(data, address);
			if (p) {
				if (checkForDups && ignoreDups(p, data, address)) {
					return;
				}
				lock();
				q.push_front(p); //bugbub do we want to add a priority? front & back? not sure
				unlock();
			}
		}
	}
	void ReadOsc::setup(int port) {
		receiver.setup(port);
		startThread();
	}

	void ReadOsc::threadedFunction() {
		while (1) {
			// check for waiting messages
			while (receiver.hasWaitingMessages()) {
				shared_ptr<ofxOscMessage> p = std::make_shared<ofxOscMessage>();
				if (p) {
					receiver.getNextMessage(*p);
					lock();
					q[p->getAddress()].push_front(p); // will create a new queue if needed
					unlock();
				}
			}
			ofSleepMillis(10);
		}
	}
	shared_ptr<ofxJSON> ReadOsc::get(const string&address) {
		shared_ptr<ofxJSON> j = nullptr;
		if (q.size() > 0) {
			lock();
			MessageMap::iterator m = q.find(address);
			if (m != q.end() && m->second.size() > 0) {
				j = OSCMessage::toJson((m->second).back());
				m->second.pop_back();// first in first out
			}
			unlock();
		}
		return j;
	}

	void TCPServer::setup() {
		
		server.setup(11999);
		// optionally set the delimiter to something else.  The delimiter in the client and the server have to be the same, default being [/TCP]
		//server.setMessageDelimiter("\n");

		startThread();
	}
	// input data is  deleted by this object at the right time (at least that is the plan)
	void TCPServer::update(const char * bytes, const size_t numBytes, char type, int clientID) {
		string buffer;
		if (compress(bytes, numBytes, buffer)) { // copy and compress data so caller can free passed data 
			//buffer = "hi";
			char *bytes2 = new char[sizeof(TCPMessage) + buffer.size()];
			if (bytes2) {
				TCPMessage* m = (TCPMessage*)bytes2;
				m->numberOfBytes = sizeof(TCPMessage)+ buffer.size()-1;
				m->bytesSize = buffer.size();
				m->type = type;
				m->clientID = clientID;
				m->t = 's';
				memcpy_s(m->bytes, m->bytesSize, buffer.c_str(), buffer.size()); // hate the double buffer move bugbug go to source/sync?
				lock();
				q.push_front(m); //bugbub do we want to add a priority? front & back? not sure
				unlock();
			}
		}
	}

	// control data deletion (why we have our own thread) to avoid data copy since this code is in a Kinect crital path 
	void TCPServer::threadedFunction() {
		while (1) {
			if (q.size() > 0) {
				lock();
				TCPMessage* m = q.front();
				q.pop_front();
				unlock();
				if (m) { 
					sendbinary(m);
					delete m;
				}
			}
			ofSleepMillis(10);
		}
	}
	void TCPServer::sendbinary(TCPMessage *m) {
		if (m) {
			if (m->numberOfBytes > MAXSEND) {
				ofLogError("TCPServer::sendbinary") << "block too large " << m->numberOfBytes + " max " << MAXSEND;
				return;
			}

			if (m->clientID > 0) {
				server.sendRawBytes(m->clientID, (const char*)m, m->numberOfBytes);
			}
			else {
				server.sendRawBytesToAll((const char*)m,  m->numberOfBytes);
			}
		}
		// we throttle the message sending frequency to once every 100ms
		ofSleepMillis(90);
	}
	char TCPClient::update(string& buffer) {
		char type = 0;
		if (tcpClient.isConnected()) {
			lock();// need to read one at a time to keep input organized
			TCPMessage* msg = (TCPMessage*)std::malloc(sizeof(TCPMessage));
			if (!msg) {
				unlock();
				return 0;
			}
			int messageSize = tcpClient.peekReceiveRawBytes((char*)msg, sizeof(TCPMessage));
			//int messageSize = tcpClient.receiveRawBytes((char*)msg, sizeof(TCPMessage)+msg->numberOfBytes);
			if ((messageSize != sizeof(TCPMessage))) {
				free(msg);
				unlock();
				ofSleepMillis(100); // maybe things will come back
				return 0;
			}
			if (msg->t != 's') {
				free(msg);
				unlock(); // not sure where the spurious data is coming from
				return 0;
			}
			if (msg->numberOfBytes > MAXSEND) {
				ofLogError("TCPServer::sendbinary") << "block too large " << msg->numberOfBytes + " max " << MAXSEND;
				free(msg);
				unlock();
				ofSleepMillis(2000); // slow down, attack may be occuring
				return 0;
			}
			msg = (TCPMessage*)std::realloc(msg, msg->numberOfBytes);
			if (!msg) {
				ofSleepMillis(1000); // maybe things will come back
				unlock();
				return 0;
			}

			//messageSize = tcpClient.receiveRawMsg((char*)msg, msg->numberOfBytes);
			messageSize = tcpClient.receiveRawBytes((char*)msg, msg->numberOfBytes);
			// if data was read
			if (messageSize == msg->numberOfBytes) {
				if (uncompress(msg->bytes, msg->bytesSize, buffer)) {
					type = msg->type; // data should change a litte
				}
				else {
					ofLogError("data ignored");
				}
			}
			free(msg);
			unlock();
		}
		else {
			if (!tcpClient.setup("127.0.0.1", 11999)) {
				ofSleepMillis(1000);
			}
		}
		return type;
	}
	void TCPClient::setup() {

		// optionally set the delimiter to something else.  The delimiter in the client and the server have to be the same
		//tcpClient.setMessageDelimiter("\n");
	}
}