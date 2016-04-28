#pragma once

namespace Software2552 {
#define kinectWidthForColor 1920
#define kinectHeightForColor 1080
#define kinectWidthForIR 512
#define kinectHeightForIR 424
#define kinectWidthForDepth 512
#define kinectHeightForDepth 424

	// from https://github.com/ekino/ek-winjs-kinect/blob/master/platforms/windows/references/KinectImageProcessor/InfraredHelper.cpp
#define InfraredOutputValueMinimum 0.01f 
#define InfraredOutputValueMaximum 1.0f
#define InfraredSourceValueMaximum static_cast<float>(USHRT_MAX)
#define InfraredSceneStandardDeviations 3.0f
#define InfraredSceneValueAverage 0.08f

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

		void setHand(const Json::Value &data, float x, float y);
		void setFace(const Json::Value &data);
		vector <ofPoint> points; // z is radius
	};

	void bodyFromTCP(const char * bytes, const int numBytes, Kinect&);

	typedef std::unordered_map<OurPorts, shared_ptr<TCPClient>> ClientMap;

	class TCPReader : public ofThread {
	public:
		void setup(const string& ip = "192.168.1.21");
		void add(const string& ip = "192.168.1.21", OurPorts port = TCP, bool blocking = false);
		bool get(OurPorts port, string& buffer);

	private:
		void threadedFunction();
		void update(); // let the thread do this
		ClientMap clients;
	};
	
	typedef std::unordered_map<OurPorts, shared_ptr<TCPServer>> ServerMap;

	class Router{
	public:
		void setup();
		WriteOsc comms;
		void send(const char * bytes, const size_t numBytes, OurPorts port, int clientID=-1);
		void addTCP(OurPorts port = TCP, bool blocking = false);

	private:
		ServerMap servers;
	};
}