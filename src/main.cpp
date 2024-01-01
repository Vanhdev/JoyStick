#include <Arduino.h>
#include <Wire.h>
#include <L3G.h>
#include <BleMouse.h>
#include <siot_core_lib.h> // SIOT Core Lib - all packages or you could select each package manually.
#include <ezButton.h>
#include <PubSubClient.h>
#include <WiFi.h>

//-----------------------------------------------------------------------------------------------
//      CÁC THIẾT LẬP PORTING - cho biết các module linh kiện được ghép nối với CPU ở pin nào?
//-----------------------------------------------------------------------------------------------
/** GPIO điều khiển rotarymeter */
#define SDA_L3G GPIO_NUM_22
#define SCL_L3G GPIO_NUM_23
/** GPIO điều khiển rotarymeter */
#define switchPin GPIO_NUM_15
#define outputB GPIO_NUM_13
#define outputA GPIO_NUM_14

/** Thiết lập thông tin MQTT host*/
#define MQTT_SERVER "broker.emqx.io"
#define MQTT_PORT 1883

/** Thiết lập MQTT Received topic*/
#define MQTT_L3G_RECIEVE "RECEIVE L3G DATA"
#define MQTT_JOYSTICK_PUBLISH "ESP32/DATA_RESPONSE"

/** Thiết lập wifi*/
const char *ssid = "V13t4nhtr4n";
const char *password = "vietanh123";

// WiFiSelfEnroll *MyWiFi = new WiFiSelfEnroll();
WiFiClient wifiClient;
PubSubClient client(wifiClient);
L3G gyro;

int x_init, y_init, z_init;

int counter = 0; // Biến đếm số interval mà rotarymeter quay được. Số dương là thả dây, Số âm là cuộn dây
int aState;
int aLastState;

// BleKeyboard bleKeyboard;
BleMouse bleMouse;
ezButton button(switchPin);

#define ENABLE_MTQTT

#if defined(ENABLE_MTQTT)
    
#endif

void initWiFi()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	Serial.print("Connecting to WiFi ..");
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print('.');
		delay(500);
	}
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

void connect_to_broker()
{
	while (!client.connected())
	{
		Serial.print("Attempting MQTT connection...");
		String clientId = "ESP32";
		clientId += String(random(0xffff), HEX);
		if (client.connect(clientId.c_str()))
		{
			Serial.println("connected");
			client.subscribe(MQTT_L3G_RECIEVE);
		}
		else
		{
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 2 seconds");
			delay(2000);
		}
	}
}

void callback(char *topic, byte *payload, unsigned int length)
{
}

void setup()
{
	Serial.begin(115200);
	pinMode(outputA, INPUT);
	pinMode(outputB, INPUT);
	pinMode(switchPin, INPUT);
	Wire.begin(SDA_L3G, SCL_L3G);

	// Make sure WiFi ssid/password is correct. Otherwise, raise the Adhoc AP Station with ssid = SOICT_CORE_BOARD and password =  12345678
	// MyWiFi->setup("TP-Link_5BAO", "12067039");
	// Release the memory allocated for WiFi Station Handler after finishing his work
	// delete MyWiFi;
	// MyWiFi = _NULL;
	// TODO something

	initWiFi();

	client.setServer(MQTT_SERVER, MQTT_PORT);
	client.setCallback(callback);
	connect_to_broker();
	Serial.println("Start transfer");

	bleMouse.begin();
	aLastState = digitalRead(outputA);

	if (!gyro.init())
	{
		Serial.println("Failed to autodetect gyro type!");
		while (1)
			;
	}

	gyro.enableDefault();
	gyro.read();
	x_init = (int)gyro.g.x;
	y_init = (int)gyro.g.y;
	z_init = (int)gyro.g.z;
}

void publish(int x, int y, int z, int rotary, bool sw)
{
	std::string strx = std::to_string(x);
	std::string stry = std::to_string(y);
	std::string strz = std::to_string(z);
	std::string strr = std::to_string(rotary);
	std::string str = "{x:" + strx + ",y:" + stry + ",z:" + strz + ",rotary_index:" + strr + ",rotary_button:" + (sw ? "1}" : "0}");
	char *temp = new char[str.length() + 1];
	strcpy(temp, str.c_str());
	client.publish(MQTT_JOYSTICK_PUBLISH, temp);
	Serial.println(temp);
}

void loop()
{
	button.loop();
	gyro.read();

	// Serial.print("G ");
	// Serial.print("X : ");
	// Serial.print((int)gyro.g.x);
	// Serial.print(" Y : ");
	// Serial.print((int)gyro.g.y);
	// Serial.print(" Z : ");
	// Serial.println((int)gyro.g.z);
	delay(200);
	bool sw = false;
	if (bleMouse.isConnected())
	{
		int x_diff = (int)gyro.g.x - x_init;
		int y_diff = (int)gyro.g.y - y_init;
		int z_diff = (int)gyro.g.z - z_init;

		if (x_diff != 0 || y_diff != 0 || z_diff != 0)
		{
			bleMouse.move(x_diff / 100, y_diff / 100, z_diff / 100);
		}

		aState = digitalRead(outputA);

		if (aState != aLastState)
		{
			if (digitalRead(outputB) != aState)
			{
				Serial.println("Incre");
				counter++;
				Serial.println(counter);
				bleMouse.move(0, 0, 0, 1);
			}
			else
			{
				Serial.println("Decre");
				counter--;
				Serial.println(counter);
				bleMouse.move(0, 0, 0, -1);
			}
		}

		if (button.isPressed())
		{
			bleMouse.click(MOUSE_RIGHT);
			sw = true;
			delay(100);
		}

		publish(gyro.g.x, gyro.g.y, gyro.g.z, counter, sw);
		aLastState = aState;
		x_init = x_diff;
		y_init = y_diff;
		z_init = z_diff;
		delay(50);
	}
}