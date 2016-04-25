#include "ofApp.h"
#include "control.h"

namespace Software2552 {
#define MAXSEND (512*30)

	shared_ptr<ofxJSON> OSCMessage::toJson(shared_ptr<ofxOscMessage> m) {
		if (m) {
			// bugbug data comes from one of http/s, string or local file but for now just a string
			shared_ptr<ofxJSON> p = std::make_shared<ofxJSON>();
			if (p) {
				string output;
				string input = m->getArgAsString(0);
				p->parse(uncompress(input.c_str(), input.size(), output));
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
			p->addStringArg(compress(input.c_str(), input.size(), output)); // all data is in json
		}
		return p;
	}
	WriteOsc::WriteOsc() {
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
	ReadOsc::ReadOsc() {
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
		server.setMessageDelimiter("\n");
	}
	// input data is  deleted by this object at the right time (at least that is the plan)
	void TCPServer::update(const char * bytes, const int numBytes, char type, int clientID) {
		Message* m = new Message;
		if (m) {
			m->type = type;
			m->bytes = bytes;
			m->numberOfBytes = numBytes;
			m->clientID = clientID;
			lock();
			q.push_front(m); //bugbub do we want to add a priority? front & back? not sure
			unlock();
		}
	}

	// control data deletion (why we have our own thread) to avoid data copy since this code is in a Kinect crital path 
	void TCPServer::threadedFunction() {
		while (1) {
			if (q.size() > 0) {
				lock();
				Message* m = q.front();
				q.pop_front();
				unlock();
				if (m) {
					sendbinary(m);
					if (m->bytes) {
						delete m->bytes;
					}
					delete m;
				}
			}
			ofSleepMillis(10);
		}
	}
	void TCPServer::sendbinary(Message *m) {
		if (m) {
			if (m->numberOfBytes > MAXSEND) {
				ofLogError("TCPServer::sendbinary") << "block too large " << ofToString(m->bytes);
				return;
			}
			if (m->clientID > 0) {
				server.sendRawBytes(m->clientID, m->bytes, m->numberOfBytes);
			}
			else {
				server.sendRawBytesToAll(m->bytes, m->numberOfBytes);
			}
		}
		// we throttle the message sending frequency to once every 100ms
		ofSleepMillis(90);
	}
	char TCPClient::update(string& buffer) {
		char type = 0;
		if (tcpClient.isConnected()) {
			//return or turn into json/ uncompress etc
			char buf[MAXSEND]; // we control the send
			int messageSize;
			std::size_t buffersize = 0;
			int size = 0;
			string readdata;
			readdata.resize(MAXSEND); // try to save on string reallocs
			// never send more than 20k, but watch for over flow attacks
			while ((messageSize = tcpClient.receiveRawBytes(buf, MAXSEND)) > 1) {
				size += messageSize;
				readdata.append(buf, messageSize);
			}
			if (readdata.size() > 0) {
				type = readdata[readdata.size() - 1]; // type was tacked on to the end after compression
				readdata.resize(size - 1);//bugbug does this remove it?
				uncompress(readdata.c_str(), readdata.size(), buffer);
			}
		}
		return type;
	}
	void TCPClient::setup() {

		// connect to the server - if this fails or disconnects
		// we'll check every few seconds to see if the server exists
		tcpClient.setup("127.0.0.1", 11999);

		// optionally set the delimiter to something else.  The delimiter in the client and the server have to be the same
		tcpClient.setMessageDelimiter("\n");
	}
}