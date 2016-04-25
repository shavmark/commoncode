#include "ofApp.h"
#include "control.h"

namespace Software2552 {

void Router::setup() {
	server.setup();
	comms.setup();
}
void Router::update() { 
}
void Router::sendBodyIndex(string *s, int clientID) {
	if (s) {
		s += BODYINDEX; // s is already compressed does this work?
		server.update(s->c_str(), s->size(), clientID);
	}
}
void Router::sendIR(string *s, int clientID) {
	if (s) {
		s += IR; // s is already compressed does this work?
		server.update(s->c_str(), s->size(), clientID);
	}
}

void TCPReader::setup() {
	client.setup();
}
void TCPReader::update() {
	string buffer;
	ofImage image;//bugbug convert to a an item for our drawing queue

	switch (client.update(buffer)) {
	case 0:
		break;
	case IR:
		IRFromTCP(buffer, image);
		break;
	case BODY:
		break;
	case BODYINDEX:
		bodyIndexFromTCP(buffer, image);
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
