#include "ofApp.h"
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
		return size > 0;
	}

	// input buffer returned as reference
	bool uncompress(const char*buffer, size_t len, string&output) {
		if (!snappy::Uncompress(buffer, len, &output)) {
			ofLogError("uncompress") << "fails";
			return false;
		}
		return true;
	}
	void Sender::setup() {
		addTCPServer(TCP, true);
		addTCPServer(TCPKinectIR, true);
		addTCPServer(TCPKinectBodyIndex, true);
		addTCPServer(TCPKinectBody, true);
		comms.setup();
		//.41
		//broad cost sign on
	}
	void Sender::send(const char * bytes, const size_t numBytes, OurPorts port, int clientID) {
		if (numBytes > 0) {
			ServerMap::iterator s = servers.find(port);
			if (s != servers.end()) {
				s->second->update(bytes, numBytes, mapPortToType(port), clientID);
			}
		}
	}

	void TCPReader::setup(const string& ip) {
		add("192.168.1.41", TCP, true);//me .21 cheap box .41
		add("192.168.1.41", TCPKinectIR, true); //bugbug get server ip via osc broad cast or such
		add("192.168.1.41", TCPKinectBody, true);
		add("192.168.1.41", TCPKinectBodyIndex, true);
		if (!isThreadRunning()) {
			startThread();
		}
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
	void Sender::addTCPServer(OurPorts port, bool blocking) {
		shared_ptr<TCPServer> s = std::make_shared<TCPServer>();
		if (s) {
			s->setup(port, blocking);
			servers[port] = s; // will create a new queue if needed
		}
	}

	// get data, if any
	bool TCPReader::get(OurPorts port, shared_ptr<ReadTCPPacket> packet) {

		ClientMap::iterator c = clients.find(port);
		if (c != clients.end()) {
			packet = c->second->get();
			if (packet) {
				if (packet->type == mapPortToType(port)) {
					return true;
				}
				else {
					ofLogError("TCPReader::get()") << " wrong type " << packet->type << "for port " << port;
				}
			}
		}
		return false;
	}

	void TCPReader::update() {
		ofImage image;//bugbug convert to a an item for our drawing queue
		shared_ptr<ReadTCPPacket> packet;
		Kinect kinect;

		// this code is designed to read all set connections, validate data and process it
		if (get(TCPKinectBodyIndex, packet)) {
			bodyIndexFromTCP(packet->data.c_str(), packet->data.size(), image);
		}

		if (get(TCPKinectBody, packet)) {
			bodyFromTCP(packet->data.c_str(), packet->data.size(), kinect);
		}

		if (get(TCPKinectIR, packet)) {
			IRFromTCP((const UINT16 *)packet->data.c_str(), image);
		}

		if (get(TCP, packet)) {
			;// not defined yet
		}

	}
	// call on every update (this is done on the client side, not the server side)
	void bodyIndexFromTCP(const char * bytes, const int numBytes, ofImage& image) {
		if (numBytes == 0) {
			return;
		}

		image.clear();
		image.allocate(kinectWidthForDepth, kinectHeightForDepth, OF_IMAGE_COLOR);
		for (float y = 0; y < kinectHeightForDepth; y++) {
			for (float x = 0; x < kinectWidthForDepth; x++) {
				unsigned int index = y * kinectWidthForDepth + x;
				if (((unsigned char*)bytes)[index] != 0xff) {
					float hue = x / kinectWidthForDepth * 255;
					float sat = ofMap(y, 0, kinectHeightForDepth / 2, 0, 255, true);
					float bri = ofMap(y, kinectHeightForDepth / 2, kinectHeightForDepth, 255, 0, true);
					// make a dynamic image, also there can be up to 6 images so we need them to be a little different 
					image.setColor(x, y, ofColor::fromHsb(hue, sat, bri));
				}
				else {
					image.setColor(x, y, ofColor::white);
				}
			}
		}
		image.update();
	}

	// size is fixed
	void IRFromTCP(const UINT16 * bytes, ofImage& image) {
		image.clear();
		image.allocate(kinectWidthForIR, kinectHeightForIR, OF_IMAGE_COLOR);
		for (float y = 0; y < kinectHeightForIR; y++) {
			for (float x = 0; x < kinectWidthForIR; x++) {
				unsigned int index = y * kinectWidthForIR + x;
				image.setColor(x, y, ofColor().fromHsb(255, 255,bytes[index]));
			}
		}
		image.update();
	}
	void Face::update(const Json::Value &data) {
		points.clear();
		elipses.clear();

		if (data.size() == 0) {
			return;
		}
		//bugbug figure out a way to scale to all screens, like do the ratio of 1000x1000 are the values we use then mult by scale, not sure, but its
		// drawing thing more than a data thing
		Json::Value::Members m = data.getMemberNames();

		if (m.size() < 3) {
			return; // not enough data to matter
		}
		float sw = ofGetScreenWidth();
		float ratioX = (sw / kinectWidthForColor);
		float sh = ofGetScreenHeight();
		float ratioY = (sh / kinectHeightForColor);

		// setup upper left
		rectangle.set(data["boundingBox"]["left"].asFloat()*ratioX, data["boundingBox"]["top"].asFloat()*ratioY,
			data["boundingBox"]["right"].asFloat()*ratioX - data["boundingBox"]["left"].asFloat()*ratioX,
			data["boundingBox"]["bottom"].asFloat()*ratioY - data["boundingBox"]["top"].asFloat()*ratioY);

		pitch = data["rotation"]["pitch"].asFloat();
		yaw = data["rotation"]["yaw"].asFloat();
		roll = data["rotation"]["roll"].asFloat(); // bugbug rotate this in 3d

		float x = data["eye"]["left"]["x"].asFloat() * ratioX;
		float y = data["eye"]["left"]["y"].asFloat() * ratioY;

		float radius = (data["eye"]["left"]["closed"].asBool()) ? 15 : 25;
		elipses.push_back(ofVec4f(x, y, 10, 20));

		x = data["eye"]["right"]["x"].asFloat()*ratioX;
		y = data["eye"]["right"]["y"].asFloat()*ratioY;
		radius = (data["face"]["eye"]["right"]["closed"].asBool()) ? 5 : 10;
		elipses.push_back(ofVec4f(x, y, 10, 20));

		x = data["nose"]["x"].asFloat()*ratioX;
		y = data["nose"]["y"].asFloat()*ratioY;
		elipses.push_back(ofVec4f(x, y, 10, 15));

		x = data["mouth"]["left"]["x"].asFloat()*ratioX;
		y = data["mouth"]["left"]["y"].asFloat()*ratioY;
		float x2 = data["mouth"]["right"]["x"].asFloat()*ratioX;
		float y2 = data["mouth"]["right"]["y"].asFloat()*ratioY;

		if (data["happy"].asBool()) {
			elipses.push_back(ofVec4f(x, y, 15, 50));//bugbug for now just one circule, make better mouth later
		}
		else if (data["mouth"]["open"].asBool()) {
			elipses.push_back(ofVec4f(x, y, abs(x - x2), 10));//bugbug for now just one circule, make better mouth later
		}
		else if (data["mouth"]["moved"].asBool()) {
			elipses.push_back(ofVec4f(x, y, 5 + abs(x - x2), ofRandom(15)));//bugbug for now just one circule, make better mouth later
		}
		else {
			points.push_back(ofPoint(x, y, 15));
			points.push_back(ofPoint(x2, y2, 15));
		}

	}
	void Face::setup() {
	}
	// draw face separte from body
	void Face::draw() {
		ofSetColor(ofColor::blue); //bugbug clean changing up to fit in with rest of app, also each user gets a color
		ofPushStyle();
		//ofFill();
		if (rectangle.width != 0) {
			ofPushMatrix();
			//ofTranslate(rectangle.width / 2, rectangle.height / 2, 0);//move pivot to centre
			//ofRotateZ(yaw); // bugbug figure out rotation
			//ofTranslate(rectangle.width/2, rectangle.height/2);
			//ofRotateX(roll);
			//ofRotateY(pitch);
			ofPushMatrix();
			//ofTranslate(-rectangle.width / 2, -rectangle.height / 2);
			ofDrawRectRounded(rectangle, 5);
			ofPopMatrix();
			//ofRotateX(pitch);
			//ofPushMatrix();
			//rectangle.draw(-rectangle.width / 2, -rectangle.height / 2);//move back by the centre offset
			ofPopMatrix();
			//ofPopMatrix();
		}
		ofSetColor(ofColor::red); //bugbug clean changing up to fit in with rest of app, also each user gets a color
		for (const auto&face : points) {
			ofDrawCircle(face.x, face.y, face.z); // z is radius
		}
		ofSetColor(ofColor::green); //bugbug clean changing up to fit in with rest of app, also each user gets a color
		for (const auto&e : elipses) {
			ofDrawEllipse(e.x, e.y, e.z, e.w);
		}
		ofPopStyle();

	}
	void Kinect::setup() {
	}
	// need to add this to our model etc
	void Kinect::draw() {
		ofColor color = ofColor::orange;
		
		ofNoFill();
		for (const auto&circle : points) {
			ofSetColor(color); //bugbug clean changing up to fit in with rest of app
			color.setHue(color.getHue() + 6.0f);
			ofDrawCircle(circle.x, circle.y, circle.z);
		}

	}
	void Kinect::setFace(const Json::Value &data) {
		face.update(data);
	}

	void Kinect::setHand(const Json::Value &data, float x, float y, float size) {
		if (data["state"] == "open") {
			points.push_back(ofPoint(x, y, size*2));
		}
		else if (data["state"] == "lasso") {
			points.push_back(ofPoint(x, y, size));
		}
		else if (data["state"] == "closed") {
			points.push_back(ofPoint(x, y, size/4));
		}
	}
	void Kinect::update(ofxJSON& data) {
		points.clear();
		float ratioX = ((float)ofGetScreenWidth() / kinectWidthForDepth);
		float ratioY = ((float)ofGetScreenHeight() / kinectHeightForDepth);

		for (Json::ArrayIndex i = 0; i < data["body"].size(); ++i) {
			face.update(data["body"][i]["face"]);
			Json::Value::Members m = data["body"][i].getMemberNames();
			for (Json::ArrayIndex j = 0; j < data["body"][i]["joint"].size(); ++j) {
				float x = data["body"][i]["joint"][j]["depth"]["x"].asFloat() * ratioX; // using depth coords which will not match the face
				float y = data["body"][i]["joint"][j]["depth"]["y"].asFloat() * ratioY;

				if (data["body"][i]["joint"][j]["jointType"] == JointType::JointType_HandRight) {
					setHand(data["body"][i]["joint"][j]["right"], x, y, ratioX * 25);
				}
				else if (data["body"][i]["joint"][j]["jointType"] == JointType::JointType_HandLeft) {
					setHand(data["body"][i]["joint"][j]["left"], x, y, ratioX * 25);
				}
				else if (data["body"][i]["joint"][j]["jointType"] == JointType::JointType_Head) {
					points.push_back(ofPoint(x, y, ratioX * 25));// bugbug add color etc
				}
				else {
					// just the joint gets drawn, its name other than JointType_Head (hand above head)
					// is not super key as we track face/hands separatly 
					points.push_back(ofPoint(x, y, ratioX * 10));// bugbug add color etc
				}
			}
		}
	}
	void bodyFromTCP(const char * bytes, const int numBytes, Kinect& body) {
		ofxJSON data;
		if (!data.parse(bytes)) {
			ofLogError("bodyFromTCP") << "invalid json " << bytes;// lets hope its null terminated
			return;
		}
		body.update(data);
	}

}