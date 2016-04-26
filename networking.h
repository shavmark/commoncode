#pragma once
//how to broad cast https://forum.openframeworks.cc/t/fixed-ofxosc-broadcast-to-255-255-255-255-in-of-0-84-under-windows/21974
//https://github.com/microcosm/KinectV2-OSC Address: /bodies/{bodyId}/joints/{jointId}
//Values: -float : positionX
	//- float : positionY
	//- float : positionZ
	//- string : trackingState(Tracked, NotTracked or Inferred)
#include "ofApp.h"
#include "ofxOsc.h"
#include "ofxNetwork.h"


	// phase II go to something like IoTivity or AllJoyn, let the dust settle.  

// notes: remote files can use ofImage.loadImage("http://some.com/url.png")
namespace Software2552 {
	class OSCMessage {
	public:
		static shared_ptr<ofxJSON> toJson(shared_ptr<ofxOscMessage>);
		static shared_ptr<ofxOscMessage> fromJson(ofxJSON &data, const string&address);
	};
	struct TCPMessage {
		size_t numberOfBytes;// used to cross check data
		char type;
		int clientID;
		char t = 's'; // just a data check
		size_t bytesSize;// used to cross check data
		char bytes[1];//compressed
	};

	// deque allows push front and back and enumration so we do priorities and remove old data
	typedef std::unordered_map<string, deque<shared_ptr<ofxOscMessage>>>MessageMap;

	class WriteOsc : public ofThread {
	public:
		void setup(const string &hostname = "192.168.1.255", int port = 2552);

		// add a message to be sent
		void update(ofxJSON &data, const string&address);

	private:
		void threadedFunction();
		bool ignoreDups(shared_ptr<ofxOscMessage> p, ofxJSON &data, const string&address);
		bool checkForDups = false;
		ofxOscSender sender;
		deque<shared_ptr<ofxOscMessage>> q;
		deque<shared_ptr<ofxOscMessage>> memory; // used to avoid dups, saves last 200 messages bugbug check this number
	};

	class ReadOsc : public ofThread {
	public:
		void setup(int port = 2552);

		shared_ptr<ofxJSON> get(const string&address);

	private:
		void threadedFunction();
		ofxOscReceiver receiver;
		MessageMap q;
	};

	class TCPServer : public ofThread {
	public:
		void setup();
#define BODYINDEX 'x'
#define IR 'i'
#define BODY 'b'
#define JSON 'j'
		// type B(BodyIndex), I(IR), X(BodyIndex)
		void update(const char * rawBytes, const size_t numBytes, char type, int clientID = -1);
	private:
		void threadedFunction();
		void sendbinary(TCPMessage *m);
		ofxTCPServer server;
		deque<TCPMessage*> q;
	};
	class TCPClient : ofThread {
	public:
		void setup();
		char update(string& buffer);
	private:
		ofxTCPClient tcpClient;
	};
}