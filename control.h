#pragma once

namespace Software2552 {
#define kinectWidthForColor 1920
#define kinectHeightForColor 1080
#define kinectWidthForIR 512
#define kinectHeightForIR 424
#define kinectWidthForDepth 512
#define kinectHeightForDepth 424

	bool compress(const char*buffer, size_t len, string&output);
	bool uncompress(const char*buffer, size_t len, string&output);
	void bodyIndexFromTCP(const char * bytes, const int numBytes, ofImage& image);
	void IRFromTCP(const UINT16 * bytes, ofImage& image);
	PacketType mapPortToType(OurPorts ports);

	class Face {
	public:
		void draw();
		void update(const Json::Value &data);
		void setup();
	private:
		vector <ofPoint> points;
		vector <ofVec4f> elipses;
		ofRectangle rectangle;
		float pitch = 0;
		float yaw = 0;
		float roll = 0;

	};
	class Kinect {
	public:
		void draw();
		void update(ofxJSON& data);
		void setup();
		Face face;
	private:

		void setHand(const Json::Value &data, float x, float y, float size);
		void setFace(const Json::Value &data);
		vector <ofPoint> points; // z is radius
	};

	void bodyFromTCP(const char * bytes, const int numBytes, Kinect&);

	typedef std::unordered_map<OurPorts, shared_ptr<TCPClient>> ClientMap;

	class TCPReader : public ofThread {
	public:
		void setup(const string& ip = "192.168.1.41");
		void add(const string& ip = "192.168.1.41", OurPorts port = TCP, bool blocking = false);
		bool get(OurPorts port, shared_ptr<ReadTCPPacket> packet);

	private:
		void threadedFunction();
		void update(); // let the thread do this
		ClientMap clients;
	};
	
	typedef std::unordered_map<OurPorts, shared_ptr<TCPServer>> ServerMap;

	class Sender{
	public:
		void setup();
		WriteOsc comms;
		void send(const char * bytes, const size_t numBytes, OurPorts port, int clientID=-1);
		void addTCPServer(OurPorts port = TCP, bool blocking = false);
		bool tcpEnabled();
		bool kinectIREnabled();
		bool KinectBodyIndexEndabled();
		bool KinectBodyEnabled();
		bool enabled(OurPorts port);
	private:
		ServerMap servers;
	};
}