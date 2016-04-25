#pragma once


namespace Software2552 {
	string& compress(const char*buffer, size_t len, string&output);
	string& uncompress(const char*buffer, size_t len, string&output);

	class TCPReader {
	public:
		void setup();
		void update();
		void bodyIndexFromTCP(const string& buffer, ofImage& image);
		void IRFromTCP(const string& buffer, ofImage& image);
		void bodyFromTCP(const string& buffer);
	private:
		TCPClient client;
	};
	class Router{
	public:
		void setup();
		void update();
		WriteOsc comms;
		void send(string &, char type, int clientID=-1);
	private:
		TCPServer server;
	};
}