#include "ofApp.h"
#include "control.h"
#include "snappy.h"
#include "inc\Kinect.h" // needed for enums

namespace Software2552 {

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
	server.setup();
	comms.setup();
}
void Router::send(const char * bytes, const size_t numBytes, char type, int clientID) {
	if (numBytes > 0) {
		server.update(bytes, numBytes, type, clientID);
	}
}

void TCPReader::setup() {
	client.setup();
	startThread();
}
void TCPReader::threadedFunction() {
	while (1) {
		update();
		yield();
	}
}

void TCPReader::update() {
	string buffer;
	ofImage image;//bugbug convert to a an item for our drawing queue
	///bug need to queue and have the main thread draw, which is what we do:
	//For example, OpenGL can only run in the main execution thread.
	switch (client.update(buffer)) {
	case 0:
		break;
	case IR:
		//IRFromTCP(buffer, image);
		break;
	case JSON: // raw jason
		break;
	case BODY:
		//bodyFromTCP(buffer);
		break;
	case BODYINDEX:
		//bodyIndexFromTCP(buffer, image);
		break;
	default:
		break;
	}
}
void TCPReader::IRFromTCP(const string& buffer, ofImage& image) {
	if (buffer.size() == 0) {
		return;
	}
	int width = 512; //bugbug constants
	int height = 424;
	image.allocate(width, height, OF_IMAGE_COLOR);
	for (float y = 0; y < height; y++) {
		for (float x = 0; x < width; x++) {
			unsigned int index = y * width + x;
			image.setColor(x, y, ofColor::fromHsb(255, 50, buffer[index]));
		}
	}
	image.update();
}
void TCPReader::bodyFromTCP(const string& buffer) {
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
void TCPReader::bodyIndexFromTCP(const string& buffer, ofImage& image) {
		if (buffer.size() == 0) {
			return;
		}

		string bufferobject;
		string s;
		int width = 512;
		int height = 424;

		unsigned char*b2 = (unsigned char*)buffer.c_str();
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
