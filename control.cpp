#include "ofApp.h"
#include "control.h"
#include "snappy.h"
#include "inc\Kinect.h" // needed for enums

namespace Software2552 {
	PacketType mapPortToType(OurPorts ports) {
		switch (ports) {
		case TCP:
			return TCPID;
		case TCPKinectIR:
			return IrID;
		case TCPKinectBodyIndex:
			return BodyIndexID;
		case TCPKinectBody:
			return BodyID;
		}
	}

// input buffer returned as reference
bool compress(const char*buffer, size_t len, string&output) {
	size_t size = snappy::Compress((const char*)buffer, len, &output);
	if (size <= 0) {
		ofLogError("compress") << "fails " << size;
	}
	return size  > 0;
}

// input buffer returned as reference
bool uncompress(const char*buffer, size_t len, string&output) {
	if (!snappy::Uncompress(buffer, len, &output)) {
		ofLogError("uncompress") << "fails";
		return false;
	}
	return true;
}
void Router::setup() {
	addTCP(TCP, true);
	addTCP(TCPKinectIR, true);
	addTCP(TCPKinectBodyIndex, true);
	addTCP(TCPKinectBody, true);

	comms.setup();
}
void Router::send(const char * bytes, const size_t numBytes, OurPorts port, int clientID) {
	if (numBytes > 0) {
		ServerMap::iterator s = servers.find(port);
		if (s != servers.end()) {
			s->second->update(bytes, numBytes, mapPortToType(port), clientID);
		}
	}
}

void TCPReader::setup(const string& ip) {
	add(); // add a default client
	add("192.168.1.21", TCPKinectIR, true); //bugbug get server via osc broad cast or such
	add("192.168.1.21", TCPKinectBody, true); 
	add("192.168.1.21", TCPKinectBodyIndex, true); 
}
void TCPReader::threadedFunction() {
	while (1) {
		update();
		yield();
	}
}

void TCPReader::add(const string& ip, OurPorts port, bool blocking) {
	shared_ptr<TCPClient> c = std::make_shared<TCPClient>();
	if (c) {
		c->setup(ip, TCP, blocking);
		clients[port] = c;
	}
}
void Router::addTCP(OurPorts port, bool blocking) {
	shared_ptr<TCPServer> s = std::make_shared<TCPServer>();
	if (s) {
		s->setup(port, blocking);
		servers[port] = s; // will create a new queue if needed
	}
}

// get data
bool TCPReader::get(OurPorts port, string& buffer) {
	
	ClientMap::iterator c = clients.find(port);
	if (c != clients.end()) {
		char type = c->second->update(buffer);
		if (type == mapPortToType(port)) {
			return true;
		}
		else {
			ofLogError("TCPReader::get()") << " wrong type " << type << "for port " << port;
			return false;
		}
	}
}

void TCPReader::update() {
	ofImage image;//bugbug convert to a an item for our drawing queue
	string buffer;

	// this code is designed to read all set connections, validate data and process it
	if (get(TCPKinectBodyIndex, buffer)) {
		bodyIndexFromTCP(buffer.c_str(), buffer.size(), image);
	}

	if (get(TCPKinectBody, buffer)) {
		bodyFromTCP(buffer.c_str(), buffer.size());
	}

	if (get(TCPKinectIR, buffer)) {
		IRFromTCP(buffer.c_str(), buffer.size(), image);
	}

	if (get(TCP, buffer)) {
		;// not defined yet
	}

}
void IRFromTCP(const char * bytes, const int numBytes, ofImage& image) {
	if (numBytes == 0) {
		return;
	}
	int width = 512; //bugbug constants
	int height = 424;
	image.allocate(width, height, OF_IMAGE_COLOR);
	for (float y = 0; y < height; y++) {
		for (float x = 0; x < width; x++) {
			unsigned int index = y * width + x;
			if (index < numBytes) {
				image.setColor(x, y, ofColor::fromHsb(255, 50, bytes[index]));

			}
		}
	}
	image.update();
}
void bodyFromTCP(const char * bytes, const int numBytes) {
	// get the json from the buffer
	Json::Value data;

	int x, y;

	//data["highConfidence"]
	if (data["jointype"] == JointType::JointType_HandRight) {
	}

	if (data["state"] == "open") {
		ofDrawCircle(x, y, 30); // make these drawing types and add them in, could make them 3d for example
	}
	else if (data["state"] == "closed") {
		ofDrawCircle(x, y, 5);
	}
	else if (data["state"] == "lasso") {
		ofDrawEllipse(x, y, 15, 10);
	}

}

// call on every update (this is done on the client side, not the server side)
void bodyIndexFromTCP(const char * bytes, const int numBytes, ofImage& image) {
		if (numBytes == 0) {
			return;
		}

		string bufferobject;
		string s;
		int width = 512;
		int height = 424;

		unsigned char*b2 = (unsigned char*)bytes;
		image.allocate(width, height, OF_IMAGE_COLOR);
		bool found = false;
		for (float y = 0; y < height; y++) {
			for (float x = 0; x < width; x++) {
				unsigned int index = y * width + x;
				if (b2[index] != 0xff) {
					float hue = x / width * 255;
					float sat = ofMap(y, 0, height / 2, 0, 255, true);
					float bri = ofMap(y, height / 2, height, 255, 0, true);
					// make a dynamic image, also there can be up to 6 images so we need them to be a little different 
					image.setColor(x, y, ofColor::fromHsb(hue, sat, bri));
					found = true;
				}
				else {
					image.setColor(x, y, ofColor::white);
				}
			}
		}
		if (!found) {
			image.clear();
		}
		image.update();
	}
}
