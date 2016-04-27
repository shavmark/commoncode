#pragma once


namespace Software2552 {
	bool compress(const char*buffer, size_t len, string&output);
	bool uncompress(const char*buffer, size_t len, string&output);
	void bodyIndexFromTCP(const char * bytes, const int numBytes, ofImage& image);
	void IRFromTCP(const UINT16 * bytes, ofImage& image);
	PacketType mapPortToType(OurPorts ports);

	class Kinect {
	public:
		void draw();
		void setup(ofxJSON& data);
	private:
		void setHand(const Json::Value &data, float x, float y);
		void setFace(const Json::Value &data);
		vector <ofPoint> circles; // z is radius
		vector <ofPoint> faceitems; 
		vector <ofVec4f> elipses;
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