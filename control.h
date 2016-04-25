#pragma once


namespace Software2552 {
	string& compress(const char*buffer, size_t len, string&output);
	string& uncompress(const char*buffer, size_t len, string&output);

	class TCPReader : public ofThread {
	public:
		void setup();
		void update();
		void threadedFunction();

	private:
		void bodyIndexFromTCP(const string& buffer, ofImage& image);
		void IRFromTCP(const string& buffer, ofImage& image);
		void bodyFromTCP(const string& buffer);
		TCPClient client;
	};
	class Router{
	public:
		void setup();
		void update();
		WriteOsc comms;
		void send(const char * bytes, const size_t numBytes, char type, int clientID=-1);
	private:
		TCPServer server;
	};
}