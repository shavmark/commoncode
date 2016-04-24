#pragma once
#include <winsock.h>
#include "ofApp.h"
#include "ofxNetwork.h"
namespace Software2552 {

	class TCPServer {
	public:
	private:
		ofxTCPServer TCP; // keep private to avoid massive header file war with osc stuff

		vector <string> storeText;
		uint64_t lastSent;
	};
}

