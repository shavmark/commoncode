#include "ofApp.h"

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
					yield();
				}
			}
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
				yield();
			}
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

	void TCPServer::setup(int port, bool blocking) {
		//11999
		server.setup(port, blocking);
		startThread();
	}
	// input data is  deleted by this object at the right time (at least that is the plan)
	void TCPServer::update(const char * bytes, const size_t numBytes, PacketType type, int clientID) {
		string buffer = "hello";
		if (compress(bytes, numBytes, buffer)) { // copy and compress data so caller can free passed data 
			char *bytes = new char[sizeof(TCPMessage) + buffer.size()];
			if (bytes) {
				TCPMessage *message = (TCPMessage *)bytes;
				message->clientID = -1;
				message->packet.type = type; // passed as a double check
				message->packet.b[0] = PacketFence; // help check for lost or out of sync data
				message->numberOfBytesToSend = sizeof(TCPPacket) + buffer.size();
				memcpy_s(&message->packet.b[1], buffer.size(), buffer.c_str(), buffer.size());
				lock();
				if (q.size() > 500) {
					// see if we can let send sender catch up
					ofSleepMillis(100); // let some queue drain bugbug do we need to put this writer in a thread?
				}
				q.push_front(message); //bugbub do we want to add a priority? 
				unlock();
			}
		}
		return;
	}

	// control data deletion (why we have our own thread) to avoid data copy since this code is in a Kinect crital path 
	void TCPServer::threadedFunction() {
		while (1) {
			if (q.size() > 0) {
				lock();
				TCPMessage* m = q.back();// last in first out
				q.pop_back();
				unlock();
				if (m) { 
					sendbinary(m);
					delete m;
				}
			}
			yield();
		}
	}
	void TCPServer::sendbinary(TCPMessage *m) {
		if (m) {
			if (m->numberOfBytesToSend > MAXSEND) {
				ofLogError("TCPServer::sendbinary") << "block too large " << ofToString(m->numberOfBytesToSend) + " max " << ofToString(MAXSEND);
				return;
			}
			if (server.isConnected()) {
				if (m->clientID > 0) {
					server.sendRawMsg(m->clientID, (const char*)&m->packet, m->numberOfBytesToSend);
				}
				else {
					server.sendRawMsgToAll((const char*)&m->packet, m->numberOfBytesToSend);
				}
			}
		}
	}
	void TCPClient::threadedFunction() {
		while (1) {
			update();
			yield();
		}

	}
	shared_ptr<ReadTCPPacket> TCPClient::get() {
		shared_ptr<ReadTCPPacket>p = nullptr;
		if (q.size() > 0) {
			p = q.back();// last in first out
			q.pop_back();
		}
		return p;
	}
	char TCPClient::update() {
		char type = 0;
		if (tcpClient.isConnected()) {
			lock();// need to read one at a time to keep input organized
			char* b = (char*)std::malloc(MAXSEND);
			if (b) {
				int messageSize = 0;

				do {
					// this api will write the size of the data not the size of the buffer we pass in (ouch)
					// it buffers data beteen its markets and returns 0 until all data between 
					// markers is in its buffer which it then returns.
					messageSize = tcpClient.receiveRawMsg(b, MAXSEND);
					// only occurs due to bug or hack as we never send more than MAXSEND ourself, at this point we need to crash
					if (messageSize > MAXSEND) {
						ofExit(-2); // enables DOS, the real fix is for to not over flow in receiveRawMsg bugbug
					}
					if (messageSize > 0) {
						ofLogNotice("msg") << "got packet of size " << ofToString(messageSize);
					}
					break;
				} while (1);

				if (messageSize > 0) {
					TCPPacket*p = (TCPPacket*)b;
					if (p->b[0] == PacketFence) { // basic validation
						shared_ptr<ReadTCPPacket> returnedData = std::make_shared<ReadTCPPacket>();
						if (returnedData) {
							if (uncompress(&p->b[1], messageSize - sizeof(TCPPacket), returnedData->data)) {
								type = p->type; // data should change a litte
								returnedData->type = type;
								q.push_back(returnedData);
							}
							else {
								ofLogError("data ignored");
							}
						}
					}
				}

				free(b); // only delete if data not returned
			}
			unlock();
			return 0;

		}
		else {
			setup();
		}
		return type;
	}
	void TCPClient::setup(const string& ip, int port, bool blocking) {//192.168.1.21 or 127.0.0.1
		if (!tcpClient.isConnected()) {
			if (!tcpClient.setup(ip, port, blocking)) {
				ofSleepMillis(500); // wait before a try again if failed
				tcpClient.setup(ip, port, blocking); // caller must try again beyond this
			}
			if (!isThreadRunning() && tcpClient.isConnected()) {
				startThread(); // start thread once connection is made
			}
		}
	}
}