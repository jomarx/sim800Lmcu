/*
  Simple POST client for ArduinoHttpClient library
  Connects to server once every five seconds, sends a POST request
  and a request body
  note: WiFi SSID and password are stored in config.h file.
  If it is not present, add a new tab, call it "config.h" 
  and add the following variables:
  char ssid[] = "ssid";     //  your network SSID (name)
  char pass[] = "password"; // your network password
  created 14 Feb 2016
  by Tom Igoe
  
  this example is in the public domain
 */
#include <ArduinoHttpClient.h>
#include <ESP8266WiFi.h>
//#include <WiFi.h>

#include <SoftwareSerial.h>

//sender phone number with country code
const String PHONE = "+639175927640";

//GSM Module RX pin to NodeMCU D3
//GSM Module TX pin to NodeMCU D4
#define rxPin 2 //D4
#define txPin 0 //D3
SoftwareSerial sim800(rxPin,txPin);

#define RELAY_1 D0
#define RELAY_2 D1

String smsStatus,senderNumber,receivedDate,msg;
boolean isReply = false;

char serverAddress[] = "192.168.1.236";  // server address
int port = 80;

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, port);
int status = WL_IDLE_STATUS;
String response;
int statusCode = 0;

String contentType = "application/x-www-form-urlencoded";
String taskID = "";
String smsMsg = "";
String targetNum = "";
String Status = "";

void setup() {
	pinMode(RELAY_1, OUTPUT); //Relay 1
	pinMode(RELAY_2, OUTPUT); //Relay 2

	//digitalWrite(RELAY_1, LOW);
	//digitalWrite(RELAY_2, LOW);
	//delay(7000);

	Serial.begin(9600);
	Serial.println("NodeMCU serial initialize");

	sim800.begin(9600);
	Serial.println("SIM800L software serial initialize");

	smsStatus = "";
	senderNumber="";
	receivedDate="";
	msg="";

	sim800.print("AT+CMGF=1\r"); //SMS text mode
	delay(1000);
	
	WiFi.mode(WIFI_STA);
	
	String wfms = WiFi.macAddress();
	Serial.print("MAC address : ");
	Serial.println(wfms);
	
	String newHostname = "smsBlast01"; //This changes the hostname of the ESP8266 to display neatly on the network esp on router.
	WiFi.hostname(newHostname.c_str());
	
	wifiConnect();
	
	// print the SSID of the network you're attached to:
	Serial.println();
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());

	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print("IP Address: ");
	Serial.println(ip);
}

void loop() {
	//////////////////////////////////////////////////
	while(sim800.available()){
		parseData(sim800.readString());
	}
	//////////////////////////////////////////////////
	while(Serial.available())  {
		sim800.println(Serial.readString());
	}
	//////////////////////////////////////////////////
	
	talkPHPget ();
} //main loop ends

//***************************************************
void parseData(String buff){
	Serial.println(buff);

	unsigned int len, index;
	//////////////////////////////////////////////////
	//Remove sent "AT Command" from the response string.
	index = buff.indexOf("\r");
	buff.remove(0, index+2);
	buff.trim();
	//////////////////////////////////////////////////
	  
	  //////////////////////////////////////////////////
	if(buff != "OK"){
		index = buff.indexOf(":");
		String cmd = buff.substring(0, index);
		cmd.trim();

		buff.remove(0, index+2);

		if(cmd == "+CMTI"){
			//get newly arrived memory location and store it in temp
			index = buff.indexOf(",");
			String temp = buff.substring(index+1, buff.length()); 
			temp = "AT+CMGR=" + temp + "\r"; 
			//get the message stored at memory location "temp"
			sim800.println(temp); 

			//must insert sms to db here, status 5
		}
		else if(cmd == "+CMGR"){
			extractSms(buff);

			if(senderNumber == PHONE){
				doAction();
			}
		}
		//////////////////////////////////////////////////
	}
	else{
		//The result of AT Command is "OK"
	}
}

//************************************************************
void extractSms(String buff){
	unsigned int index;

	index = buff.indexOf(",");
	smsStatus = buff.substring(1, index-1); 
	buff.remove(0, index+2);

	senderNumber = buff.substring(0, 13);
	buff.remove(0,19);

	receivedDate = buff.substring(0, 20);
	buff.remove(0,buff.indexOf("\r"));
	buff.trim();

	index =buff.indexOf("\n\r");
	buff = buff.substring(0, index);
	buff.trim();
	msg = buff;
	buff = "";
	msg.toLowerCase();
}

void doAction(){
	if(msg == "relay1 off"){  
		digitalWrite(RELAY_1, LOW);
		Reply("Relay 1 has been OFF");
	}
	else if(msg == "relay1 on"){
		digitalWrite(RELAY_1, HIGH);
		Reply("Relay 1 has been ON");
	}
	else if(msg == "relay2 off"){
		digitalWrite(RELAY_2, LOW);
		Reply("Relay 2 has been OFF");
	}
	else if(msg == "relay2 on"){
		digitalWrite(RELAY_2, HIGH);
		Reply("Relay 2 has been ON");
	}

	smsStatus = "";
	senderNumber="";
	receivedDate="";
	msg="";  
}

void Reply(String text){
	sim800.print("AT+CMGF=1\r");
	delay(1000);
	sim800.print("AT+CMGS=\""+PHONE+"\"\r");
	delay(1000);
	sim800.println(text);
	delay(100);
	//sim800.println((char)26);// ASCII code of CTRL+Z
	sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
	delay(1000);
	Serial.println("SMS Sent Successfully.");
}

void wifiConnect () {
	// Connect to WPA/WPA2 network:
	char ssid[] = "AP Guess"; // your SSID
	char pass[] = "@pgu3$$1"; // your SSID Password

	WiFi.begin(ssid, pass);
	int ResetCounter = 0;
	Serial.print("Attempting to connect to Network ");
	while ( WiFi.status() != WL_CONNECTED) {
		
		Serial.print(".");
		ResetCounter++;
		delay(300);
		
		if (ResetCounter >= 120) {
			Serial.print("ESP8266 reset!");			

			ESP.restart();
		}
	}
}

void delayer(int dly){
	Serial.print("Delaying : ");
	for (int DelayDaw = 0; DelayDaw <= dly; DelayDaw++){
		delay(1000);
		yield();
		Serial.print(DelayDaw);
		Serial.print(".");
	}
}

void talkPHPget (){
	//get data from querytasks
	int typer=2;
	statusCode = 0;
	response = "";
	Serial.println("making POST request");
	//String contentType = "application/x-www-form-urlencoded";
	String postData = "taskType=";
	postData += typer;

	Serial.print("postData: ");
	Serial.println(postData);

	Serial.println();
	Serial.println("Start sending loop");
	
	while ( statusCode != 200) {
		
		Serial.print("x");
		//client.post("/android2/querytask.php", contentType, postData);
		client.post("/itsystemsj/querytasks.php", contentType, postData);
		// read the status code and body of the response
		statusCode = client.responseStatusCode();
		response = client.responseBody();
		Serial.print("Status code: ");
		Serial.println(statusCode);
		//200 = data sent successfully
		//-1 =
		//Response is the data the PHP server sents back
		Serial.print("Response: ");
		Serial.println(response);
		delay(100);
		//disconnect client
		client.stop();
	}
	//reset status code
	statusCode = 0;
	
	if ((response != "")&&(typer == 2)) {
	  
		
		Serial.println();
		Serial.println("not empty, means task available!");

		//crunch response to get data
		int firstCommaIndex = response.indexOf(',');
		int secondCommaIndex = response.indexOf(',', firstCommaIndex+1);
		int thirdCommaIndex = response.indexOf(',', secondCommaIndex+1);
		taskID = response.substring(0, firstCommaIndex);
		smsMsg = response.substring(firstCommaIndex+1, secondCommaIndex);
		targetNum = response.substring(secondCommaIndex+1, thirdCommaIndex);
		
		Serial.print("ID: ");
		Serial.println(taskID);
		Serial.print("smsMsg: ");
		Serial.println(smsMsg);
		Serial.print("targetNum: ");
		Serial.println(targetNum);
		Serial.println();
		
		client.stop();
		
		//send SMS
		sendSMS();
	
	}
}

void sendSMS(){
	sim800.print("AT+CMGF=1\r");
	delay(1000);
	sim800.print("AT+CMGS=\""+targetNum+"\"\r");
	delay(1000);
	sim800.println(smsMsg);
	delay(100);
	//sim800.println((char)26);// ASCII code of CTRL+Z
	sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
	delay(1000);
	Serial.println("SMS Sent Successfully.");
	
	typePhp(1); //mark SMS as done
	
	delayer(30);
}

void typePhp (int typer){
	
	statusCode = 0;
	response = "";
	Serial.println("making POST request");
	//String contentType = "application/x-www-form-urlencoded";
	String postData = "taskType=";
	postData += typer;
	
	if ((typer==1)) {
		Serial.println("Posting and updating data");
		postData += "&val1=";
		postData += taskID;
	}

	Serial.print("postData: ");
	Serial.println(postData);

	Serial.println();
	Serial.println("Start sending loop");
	
	while ( statusCode != 200) {
		
		Serial.print(".");
		//client.post("/android2/querytask.php", contentType, postData);
		client.post("/itsystemsj/querytasks.php", contentType, postData);
		// read the status code and body of the response
		statusCode = client.responseStatusCode();
		response = client.responseBody();
		Serial.print("Status code: ");
		Serial.println(statusCode);
		//200 = data sent successfully
		//-1 =
		//Response is the data the PHP server sents back
		Serial.print("Response: ");
		Serial.println(response);
		delay(100);
		
	}
	//disconnect client
		client.stop();
		
	//reset status code
	statusCode = 0;
	

    if ((typer==1)) {
		Serial.print("Update ");
		Serial.print(typer);
		Serial.print(" successful! ");
		Serial.println();
	}
  
}