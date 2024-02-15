
// *****************************************************************************
// * Amp1_Esp8266_Server                                                       *
// *    By: Tony Gargasz - tony.gargasz@gmail.com                              *
// *    WiFi Controller for changing effect modes in the AMP1 guitar amplifier *
// *    R0 - Initial development                                               *
// *    R1.2 - Changed from I2C to SPI communication type                      *
// *    R1.3 - Added Wifi comms                                                *
// *    R1.4 - Add code for D2 (espHasData) handshake to push data to Daisy    *
// *    R1.5 - Comment out 'Serial.print' statements and convert to AP mode    *
// *****************************************************************************

/*  SPI Slave - Connect Daisy to ESP8266 (SS is not used):
	GPIO   NodeMCU   Name     Color   Daisy
	=========================================
	 4       D2   espHasData   Grn    D11   
	13       D7      MOSI      Blk    D10 
	12       D6      MISO      Red    D9  
	14       D5      SCK       Wht    D8
	Note: espHasData used as ESP8266 output to tell Daisy it has new setings */

#include <hspi_slave.h>
#include <ESP8266WiFi.h>
#include <StringSplitter.h>
#include <SPISlave.h>

// Use for AP Mode
const char* ssid = "GCA-Amp1";
const char* password = "123456789";

// Use for Debug Wifi 
//const char* ssid = "KB8WOW";
//const char* password = "4407230350";

// Global vars
const int bdLed = 16;           // MCU Board LED
const int espHasData = 4;       // Connect pin D2 (GPIO4) to D11 on Daisy
boolean byp;					// Effects bypass
int flangeFilter_type;			// Flanger filter type
int Mode_Num;					// Selected effects
int Swtch1_Num;					// Footswitch assignments
int Swtch2_Num;					// Footswitch assignments
int Swtch3_Num;					// Footswitch assignments
int RateKnob_Num;				// Rate Knob assignments
int DepthKnob_Num;				// Depth Knob assignments
float delay_adj, feedback_adj;  // Delay Line (echo) vars 
float rate_adj, depth_adj;      // Tremelo vars
float gain_adj, mix_adj;        // Distortion vars
float hifreq_adj, amount_adj;   // Reverb vars
float speed_adj, range_adj;		// Flange vars	
float color_adj;				// Flange vars        

// To set timeout period
unsigned long currentMillis;
unsigned long nextMillis = 0;
const long interval = 300;  // interval at which to timeout

// Create an instance of the server
WiFiServer server(80);

void setup() 
{
	Serial.setDebugOutput(true);
	Serial.begin(115200);

	// Used to signal Daisy to receive data from ESP
	pinMode(espHasData, OUTPUT);
	pinMode(bdLed, OUTPUT);
	
	byp = false;
	Mode_Num = 7;
	Swtch1_Num = 1;
	Swtch2_Num = 2;
	Swtch3_Num = 4;
	delay_adj = 24000.00;
	rate_adj = 5.00;
	gain_adj = 0.50;
	feedback_adj = 0.60;
	depth_adj = 0.80;
	mix_adj = 0.50;
	hifreq_adj = 12500.00;
	amount_adj = 0.60;
	speed_adj = 0.20;
	range_adj = 0.75;
	color_adj = 0.75;
	flangeFilter_type = 0;
	RateKnob_Num = 1;
	DepthKnob_Num = 2;

	digitalWrite(espHasData, LOW);
	digitalWrite(bdLed, LOW);
	
	// ~~~ Start Wifi Access Point ~~~
	WiFi.mode(WIFI_AP);
	WiFi.softAP(ssid, password);

	/*
	// ~~~ Start of (Debug) WiFi Setup ~~~
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);
	
	WiFi.begin(ssid, password);
	
	while (WiFi.status() != WL_CONNECTED) 
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	*/
	
	// Start the server
	server.begin();
	Serial.println("Server started");
	// ~~~ End of WiFi Setup ~~~
	
	// Data Receive from Daisy
	SPISlave.onData(receiveData);
	
	// Start SPI Slave 
	SPISlave.begin();
}

//Receive SPI data from Daisy   Note: len is always 32 (buffer is auto-filled with zeroes)
void receiveData(uint8_t * data, size_t len)
{  
    char myData[1] = {0}; 
    String tmp = "";
    size_t DataLen;

    // Debug
    Serial.printf("From Daisy: %s\n", (char *)data);
    
	String message = String((char *)data);
	DataLen = len;

	StringSplitter *split = new StringSplitter((char*)data, ' ', 24);
	int itemCount = split->getItemCount();
	if (message.startsWith("R")) 
	{
		if (itemCount >= 4)
		{
			tmp = split->getItemAtIndex(1);
			delay_adj = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(2);
			rate_adj = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(3);
			gain_adj = strtod(tmp.c_str(), NULL);
		}
	}
	else if (message.startsWith("D")) 
	{
		if (itemCount >= 4)
		{
			tmp = split->getItemAtIndex(1);
			feedback_adj = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(2);
			depth_adj = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(3);
			mix_adj = strtod(tmp.c_str(), NULL);
		}
	}
	else if (message.startsWith("B"))
	{
		if (itemCount >= 4)
		{
			tmp = split->getItemAtIndex(1);
			hifreq_adj = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(2);
			amount_adj = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(3);
			speed_adj = strtod(tmp.c_str(), NULL);
		}
	}
	else if (message.startsWith("A"))
	{
		if (itemCount >= 4)
		{
			tmp = split->getItemAtIndex(1);
			range_adj = strtof(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(2);
			color_adj = strtof(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(3);
			flangeFilter_type = strtof(tmp.c_str(), NULL);
		}
	}
	else if (message.startsWith("M")) 
	{
		if (itemCount >= 6)
		{
			tmp = split->getItemAtIndex(1);
			Swtch1_Num = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(2);
			Swtch2_Num = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(3);
			Swtch3_Num = strtod(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(4);
			RateKnob_Num = strtof(tmp.c_str(), NULL);
			tmp = split->getItemAtIndex(5);
			DepthKnob_Num = strtof(tmp.c_str(), NULL);
		}
	}
	else if (message.startsWith("a")) 
	{
		// Daisy requesting next pack of new settings
		myData[0] = 'D';
		sendSettings(myData);
	}
	else if (message.startsWith("b"))
	{
		// Daisy requesting next pack of new settings
		myData[0] = 'B';
		sendSettings(myData);
	}
	else if (message.startsWith("c"))
	{
    	// Daisy requesting next pack of new settings
    	myData[0] = 'A';
    	sendSettings(myData);
	}
	else if (message.startsWith("d"))
	{
    	// Daisy requesting next pack of new settings
    	myData[0] = 'M';
    	sendSettings(myData);
    	digitalWrite(espHasData, LOW); // Only need to turn off one time
    	digitalWrite(bdLed, LOW);
	}
}

// Send (SPI) Settings to Daisy
void sendSettings(const char *msgType)
{
	char sndData[32] = {0};
	if (msgType[0] == 'R') 
	{
		sprintf(sndData, "R %5.3f %5.3f %5.3f",delay_adj, rate_adj, gain_adj);
		SPISlave.setData(sndData);
	}
	else if (msgType[0] == 'D') 
	{
		sprintf(sndData, "D %5.3f %5.3f %5.3f", feedback_adj, depth_adj, mix_adj);
		SPISlave.setData(sndData);
	}
	else if (msgType[0] == 'B')
	{
		sprintf(sndData, "B %5.3f %5.3f %5.3f", hifreq_adj, amount_adj, speed_adj);
		SPISlave.setData(sndData);
	}
	else if (msgType[0] == 'A')
	{
		sprintf(sndData, "A %5.3f %5.3f %1d", range_adj, color_adj, flangeFilter_type);
		SPISlave.setData(sndData);
	}
	else if (msgType[0] == 'M')
	{
		sprintf(sndData, "M %1d %1d %1d %1d %1d", Swtch1_Num, Swtch2_Num, Swtch3_Num, RateKnob_Num, DepthKnob_Num);
		SPISlave.setData(sndData);
	}
	// Debug
	Serial.printf("To Daisy: %s\n", (char *)sndData);
}

void loop() 
{
	// Timeout period is 300 ms and can be adjusted with var: interval 
	currentMillis = millis();
	if (currentMillis >= nextMillis)
	{
		if (digitalRead(espHasData))
		{
			// Set handshake low (in case Daisy never responded to data push)
			digitalWrite(espHasData, LOW);
			digitalWrite(bdLed, LOW);
			Serial.println("Daisy did not respond in a timely fashion");
		}
	}

	// Check if a client has connected
	WiFiClient client = server.available();
	if (!client) 
	{
        delay(150);
		return;
	}
  
	// Wait until the client sends some data
	Serial.println("new client");
	while (!client.available()) 
	{
		delay(100);
	}
  
	// Read the first line of the request
	String req = client.readStringUntil('\r');
	Serial.println(req);
	delay(10);
	client.flush();
	delay(100);

	/// *************** HTTP Requests ***************
	// getsets - Gets new settings from phone
	// putsets - Puts the current settings to phone
	if (req.indexOf("/getsets") != -1)
	{
		String item = "";
		StringSplitter *splitter = new StringSplitter(req, ' ', 24);
		int itemCount = splitter->getItemCount();
		if (itemCount >= 19)
		{
			item = splitter->getItemAtIndex(2);
			Swtch1_Num = strtod(item.c_str(), NULL);
			item = splitter->getItemAtIndex(3);
			Swtch2_Num = strtod(item.c_str(), NULL);
			item = splitter->getItemAtIndex(4);
			Swtch3_Num = strtod(item.c_str(), NULL);
			item = splitter->getItemAtIndex(5);
			rate_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(6);
			depth_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(7);
			delay_adj =strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(8);
			feedback_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(9);
			gain_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(10);
			mix_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(11);
			hifreq_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(12);
			amount_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(13);
			speed_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(14);
			range_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(15);
			color_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(16);
			flangeFilter_type = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(17);
			RateKnob_Num = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(18);
			DepthKnob_Num = strtof(item.c_str(), NULL);

			// Push data to Daisy through SPI
			char myData1[1] = {0};
			// Send Rate values
			myData1[0] = 'R';
			sendSettings(myData1);
			digitalWrite(espHasData, HIGH);
			digitalWrite(bdLed, HIGH);
			delay(1);
			// Daisy will return 'a' when ready to receive

			// Set the timeout timer
			nextMillis = currentMillis + interval;
		}
		else
		{
			Serial.println(" Incomplete request");
			client.stop();
			return;   
		}
	}
	else if (req.indexOf("/putsets") != -1)
	{
		if (digitalRead(espHasData))
		{
			// Set handshake low (in case Daisy never responded to a previous data push)
			digitalWrite(espHasData, LOW);
			digitalWrite(bdLed, LOW);
			Serial.println("Daisy did not respond in a timely fashion");
		}
		
		/// Fall through as a valid HTTP request
		// Just sends the current settings to phone (only happens when phone app cold starts)
	}
	else 
	{
		// Set handshake low (in case Daisy never responded to data push)
		digitalWrite(espHasData, LOW);
		digitalWrite(bdLed, LOW);
		
		Serial.println("Invalid request");
		client.stop();
		return;
	}
	client.flush();

	// Prepare the response 
	char wifiData[128] = {0};
	sprintf(wifiData, "W %1d %1d %1d %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1d %1d %1d", +
		Swtch1_Num, Swtch2_Num, Swtch3_Num, delay_adj, rate_adj, gain_adj, feedback_adj, depth_adj, mix_adj, 
		hifreq_adj, amount_adj, speed_adj, range_adj, color_adj, flangeFilter_type, RateKnob_Num, DepthKnob_Num);
	String resp = "HTTP/1.1 200 OK\r\n";
	resp += "Content-Type: text/html\r\n\r\n";
	resp +=  wifiData;
	resp += "\r\n\r\n";
		 
	// Send the response to the client
	client.print(resp);
	delay(10);

	// Debug
	Serial.printf("To Phone: %s\n", (char*)wifiData);
	// --- Client gets automatically disconnected and destroyed ---
}
