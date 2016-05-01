#include "ofApp.h"
#include "inc\Kinect.h" // needed for enums

namespace Software2552 {
	PacketType mapPortToType(OurPorts port) {
		switch (port) {
		case TCP:
			return TCPID;
		case TCPKinectIR:
			return IrID;
		case TCPKinectBodyIndex:
			return BodyIndexID;
		case TCPKinectBody:
			return BodyID;
		default:
			ofLogError("mapPortToType") << "invalid port " << port;
			return UnknownID;
		}
	}
	bool Sender::kinectIREnabled() {
		return enabled(TCPKinectIR);
	}
	bool Sender::KinectBodyIndexEndabled() {
		return enabled(TCPKinectBodyIndex);
	}
	bool Sender::KinectBodyEnabled() {
		return enabled(TCPKinectBody);
	}
	void Sender::setup() {
		Server::setup();
		addTCPServer(TCP, true);
		addTCPServer(TCPKinectIR, true);
		addTCPServer(TCPKinectBodyIndex, true);
		addTCPServer(TCPKinectBody, true);
	}

	void Client::update() {
		IRImage irImage;//bugbug convert to a an item for our drawing queue
		BodyIndexImage biImage;
		shared_ptr<ReadTCPPacket> packet;
		Kinect kinect;

		// this code is designed to read all set connections, validate data and process it
		if (get(TCPKinectBodyIndex, packet)) {
			biImage.bodyIndexFromTCP(packet->data.c_str(), packet->data.size());
		}

		if (get(TCPKinectBody, packet)) {
			kinect.bodyFromTCP(packet->data.c_str(), packet->data.size());
		}

		if (get(TCPKinectIR, packet)) {
			irImage.IRFromTCP((const UINT16 *)packet->data.c_str());
		}

		if (get(TCP, packet)) {
			;// not defined yet
		}

	}
}