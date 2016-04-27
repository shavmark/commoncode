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
	add("192.168.1.21", TCP, true); 
	add("192.168.1.21", TCPKinectIR, true); //bugbug get server ip via osc broad cast or such
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
	Kinect kinect;

	// this code is designed to read all set connections, validate data and process it
	if (get(TCPKinectBodyIndex, buffer)) {
		bodyIndexFromTCP(buffer.c_str(), buffer.size(), image);
	}

	if (get(TCPKinectBody, buffer)) {
		bodyFromTCP(buffer.c_str(), buffer.size(), kinect);
	}

	if (get(TCPKinectIR, buffer)) {
		IRFromTCP((const UINT16 *)buffer.c_str(),  image);
	}

	if (get(TCP, buffer)) {
		;// not defined yet
	}

}
// size is fixed
void IRFromTCP(const UINT16 * bytes, ofImage& image) {
	int width = 512; //bugbug constants
	int height = 424;
	image.allocate(width, height, OF_IMAGE_COLOR_ALPHA);
	for (float y = 0; y < height; y++) {
		for (float x = 0; x < width; x++) {
			unsigned int index = y * width + x;
			float brightness = bytes[index];
			image.setColor(x, y, ofColor::fromHsb(255, 255, brightness));
		}
	}
	image.update();
}
// need to add this to our model etc
void Kinect::draw() {
	ofColor color = ofColor::orange;
	ofPushStyle();
	ofNoFill();
	float scale = 2;//bugbug make this a working thing
	for (const auto&circle : circles) {
		ofSetColor(color); //bugbug clean changing up to fit in with rest of app
		color.setHue(color.getHue() + 6.0f);
		ofDrawCircle(circle.x, circle.y, circle.z*scale);
	}
	ofPopStyle();
	
	color = ofColor::blue;
	ofPushStyle();
	ofFill();
	for (const auto&face : faceitems) {
		color.setHue(color.getHue() + 8.0f);
		ofSetColor(color); //bugbug clean changing up to fit in with rest of app
		ofDrawCircle(face.x, face.y, face.z*scale);
	}
	for (const auto&e : elipses) {
		color.setHue(color.getHue() + 2.0f);
		ofSetColor(color); //bugbug clean changing up to fit in with rest of app
		ofDrawEllipse(e.x, e.y, e.z*scale, e.w*scale);
	}
	ofPopStyle();

}
void Kinect::setFace(const Json::Value &data) {
	if (data["face"].size() == 0) {
		return;
	}
	//bugbug figure out a way to scale to all screens, like do the ratio of 1000x1000 are the values we use then mult by scale, not sure, but its
	// drawing thing more than a data thing
	Json::Value::Members m = data["face"].getMemberNames();

	if (m.size() < 3) {
		return; // not enough data to matter
	}
	float pitch = data["face"]["rotation"]["pitch"].asFloat();
	float yaw = data["face"]["rotation"]["yaw"].asFloat();
	float roll = data["face"]["rotation"]["roll"].asFloat(); // bugbug rotate this in 3d

	float x = data["face"]["eye"]["left"]["x"].asFloat() - yaw;
	float y = data["face"]["eye"]["left"]["y"].asFloat() - pitch;

	float radius = (data["face"]["eye"]["left"]["closed"].asBool()) ? 15 : 25;
	elipses.push_back(ofVec4f(x, y, 10,20));

	x = data["face"]["eye"]["right"]["x"].asFloat();
	y = data["face"]["eye"]["right"]["y"].asFloat();
	radius = (data["face"]["eye"]["right"]["closed"].asBool()) ? 5 : 10;
	elipses.push_back(ofVec4f(x, y, 10, 20));

	x = data["face"]["nose"]["x"].asFloat();
	y = data["face"]["nose"]["y"].asFloat();
	elipses.push_back(ofVec4f(x, y, 10, 15));

	x = data["face"]["mouth"]["left"]["x"].asFloat();
	y = data["face"]["mouth"]["left"]["y"].asFloat();
	float x2 = data["face"]["mouth"]["right"]["x"].asFloat();
	float y2 = data["face"]["mouth"]["right"]["y"].asFloat();

	if (data["face"]["happy"].asBool()) {
		elipses.push_back(ofVec4f(x, y, 15, 50));//bugbug for now just one circule, make better mouth later
	}
	else if (data["face"]["mouth"]["open"].asBool()) {
		elipses.push_back(ofVec4f(x, y, abs(x - x2), 10));//bugbug for now just one circule, make better mouth later
	}
	else if (data["face"]["mouth"]["moved"].asBool()) {
		elipses.push_back(ofVec4f(x, y, 5+abs(x - x2), ofRandom(15)));//bugbug for now just one circule, make better mouth later
	}
	else {
		faceitems.push_back(ofPoint(x, y, 15));
		faceitems.push_back(ofPoint(x2, y2,15));
	}
}

void Kinect::setHand(const Json::Value &data, float x, float y) {
	if (data["state"] == "open") {
		circles.push_back(ofPoint(x, y, 25));
	}
	else if (data["state"] == "lasso") {
		circles.push_back(ofPoint(x, y, 15));
	}
	else if (data["state"] == "closed") {
		circles.push_back(ofPoint(x, y, 1));
	}
}
void Kinect::setup(ofxJSON& data) {
	circles.clear();
	faceitems.clear();
	elipses.clear();
	setFace(data["body"]);
	for (Json::ArrayIndex j = 0; j < data["body"]["joint"].size(); ++j) {
		float x = data["body"]["joint"][j]["color"]["x"].asFloat();
		float y = data["body"]["joint"][j]["color"]["y"].asFloat();

		if (data["body"]["joint"][j]["jointType"] == JointType::JointType_HandRight) {
			setHand(data["body"]["joint"][j]["right"], x, y);
		}
		else if (data["body"]["joint"][j]["jointType"] == JointType::JointType_HandLeft) {
			setHand(data["body"]["joint"][j]["left"], x, y);
		}
		else {
			// just the joint gets drawn, its name other than JointType_Head (hand above head)
			// is not super key as we track face/hands separatly 
			circles.push_back(ofPoint(x, y, 3));// bugbug add color etc
		}
	}
}
void bodyFromTCP(const char * bytes, const int numBytes, Kinect& body) {
	ofxJSON data;
	if (!data.parse(bytes)) {
		ofLogError("bodyFromTCP") << "invalid json " << bytes;// lets hope its null terminated
		return;
	}
	body.setup(data);
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
