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
 
 //https://github.com/ahmadlogs/arduino-ide-examples/blob/main/nodemcu-sim800l-relay/nodemcu-sim800l-relay.ino
 //https://www.robotics.org.za/SIM800L-V2
 
 
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

#define RELAY_1 D1
#define RELAY_2 D5
#define SENDSMSSW D6
#define SIM800LRST D7

//reset button values	
int swState1 = 1;

const int led = 16;
int ledStatus = 0;

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
String getSmsMsg = "";
String targetNum = "";
String Status = "";

String newHostname = "smsBlast01"; //This changes the hostname of the ESP8266 to display neatly on the network esp on router.

int devCode = 1;

void setup() {
	
	pinMode(led, OUTPUT);
	pinMode(RELAY_1, OUTPUT); //Relay 1
	pinMode(RELAY_2, OUTPUT); //Relay 2
	
	pinMode(SENDSMSSW, INPUT_PULLUP); //SENDING sms switch, pull down for off
	pinMode(SIM800LRST, INPUT_PULLUP); //sim800L reset switch

	//digitalWrite(RELAY_1, LOW);
	//digitalWrite(RELAY_2, LOW);
	//delay(7000);
	
	digitalWrite(SIM800LRST, LOW);
	delay(1000);
	digitalWrite(SIM800LRST, HIGH);

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
	for (int tempTimer = 0;tempTimer <= 7;tempTimer++)  {
		//////////////////////////////////////////////////
		while(sim800.available()){
			Serial.print("Reading SIM800L :  ");
			parseData(sim800.readString());
			delay(50);
		}
		//////////////////////////////////////////////////
		while(Serial.available())  {
			sim800.println(Serial.readString());
			delay(50);
		}
		//////////////////////////////////////////////////
		
		swState1 = digitalRead(SENDSMSSW);
		delay(50);
	}

	Serial.println("Reading button :  ");
	if (swState1 == LOW){
		//get data from PHP for message to send
		talkPHPget ();
	}
		
} //main loop ends

//***************************************************
void parseData(String buff){
	blinkLED();
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

		}
		else if(cmd == "+CMGR"){
			extractSms(buff);

			if(senderNumber == PHONE){
				Serial.println("doAction! :  ");
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
	getSmsMsg = msg;
	blinkLED();
	
	//save incoming sms to db
	typePhp(3);
	delSMS();
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
	blinkLED();
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

void delSMS(){
	Serial.println();
	sim800.print('AT+CMGDA="DEL READ"\r');
	delayer(3);
	Serial.println();
	sim800.print('AT+CMGDA="DEL SENT"\r');
	delayer(3);
	Serial.println("SMS deleted Successfully.");
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
		blinkLED();
		
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
		blinkLED();
	}
}

void talkPHPget (){
	//get data from querytasks
	blinkLED();
	int typer=2;
	statusCode = 0;
	response = "";
	Serial.println("TPG: making POST request");
	//String contentType = "application/x-www-form-urlencoded";
	String postData = "taskType=";
	postData += typer;

	Serial.print("postData: ");
	Serial.println(postData);

	Serial.println();
	Serial.println("Start sending loop");
	
	while ( statusCode != 200) {
		blinkLED();
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
	  
		blinkLED();
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
		blinkLED();
		
		delSMS();
		delayer(3);
	
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
	delay(500);
	
	for (int tempTimer = 0;tempTimer <= 100;tempTimer++)  {
		//////////////////////////////////////////////////
		while(sim800.available()){
			Serial.print("Reading SIM800L :  ");
			parseData(sim800.readString());
			delay(100);
		}
		delay(100);
	}
	
	Serial.println("SMS Sent Successfully.");
	
	delayer(5);
	
	typePhp(1); //mark SMS as done
	
	delayer(20);
}

void typePhp (int typer){
	blinkLED();
	statusCode = 0;
	response = "";
	Serial.println("TP: making POST request");
	//String contentType = "application/x-www-form-urlencoded";
	String postData = "taskType=";
	postData += typer;
	
	if ((typer==1)) {
		Serial.println("Posting and updating data");
		postData += "&val1=";
		postData += taskID;
	}
	else if ((typer==3)) {
		Serial.println("Posting and updating data");
		postData += "&val1=";
		postData += devCode;		
		postData += "&val2="; //message
		postData += getSmsMsg;		
		postData += "&val3="; //src number
		postData += senderNumber;
	}

	Serial.print("postData: ");
	Serial.println(postData);

	Serial.println();
	Serial.println("Start sending loop");
	blinkLED();
	
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
		blinkLED();
		
	}
	//disconnect client
		client.stop();
		
	//reset status code
	statusCode = 0;
	

    if ((typer==1)||(typer==3)) {
		Serial.print("Update ");
		Serial.print(typer);
		Serial.print(" successful! ");
		Serial.println();
		blinkLED();
	}
  
}

void blinkLED() {
	//
	if(ledStatus==0){
		digitalWrite(led, HIGH);
		ledStatus=1;
		delay(50);
	}
	else if(ledStatus==1) {
		//
		digitalWrite(led, LOW);
		ledStatus=0;
		delay(50);
	}

}
