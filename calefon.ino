#include "DHT11.h"
//hardware y pins
const int relayCalefon = 12;
const int pinMedidor = A2;
int pin = 11;
DHT11 dht11(pin);

//cosas del medidor de consumo
const unsigned long sampleTime = 100000UL; // sample over 100ms, it is an exact number of cycles for both 50Hz and 60Hz mains
const unsigned long numSamples = 250UL; // choose the number of samples to divide sampleTime exactly, but low enough for the ADC to keep up
const unsigned long sampleInterval = sampleTime / numSamples; // the sampling interval, must be longer than then ADC conversion time
int adc_zero; // relative digital zero of the arudino input from ACS712
int contador1 = 0;
float promedio1;
long promedio2;

//thingspeak y talkback
#define thingspeakkey "xxxx"
#define talkbackkey "xxxx"
#define talkbackid "xxxx"
#define thingspeakchannel "xxxx"

//wifi
#define SSID "xxxx"
#define PASS "xxxx"
#define IP "184.106.153.149" // thingspeak.com
String GET = "GET /update?key=";

//control por thingspeak
bool temporizadorActivado=false;
String statusTemporizador="encender";

void setup()
{
	Serial.begin(9600);
	Serial.println("AT");
	delay(5000);
	if(Serial.find("OK"))
	{
		connectWiFi();
	}
	pinMode(relayCalefon, OUTPUT);
	adc_zero = 5115; //calibrar el cero
	digitalWrite(relayCalefon, LOW);
}

void loop( )
{
	//medir consumo
	for (int temp = 0; temp <  5; temp++)
	{
		promedio1 += readCurrent(pinMedidor);
		delay(10);
	}

	if (contador1 == 149)// procesar y subir datos de consumo
	{
		int err;
		float temp, humi;
		String tempW, tempW24, tempT, tempH;
		contador1 = 0;
		promedio1 *= 220;
		promedio1 /= 750;
		if(promedio1 < 10) promedio1 = 0;
		promedio2 /= 750;
		adc_zero = promedio2;
		promedio2 = 0;
		
		if((err = dht11.read(humi, temp)) == 0)
		{
			tempT = convertirFloat(temp);
			tempH = convertirFloat(humi);
		}else{
			tempT="0";
			tempH="0";
		}
		
		tempW = convertirFloat(promedio1);
		tempW24 = convertirFloat (promedio1*24);
		promedio1 = 0;
		subirConsumo(tempW, tempW24, tempT, tempH);
	}
	else contador1++;

	//revisar talk back
	if(contador1 == 70)
	{
		String comando = revisarTalkBack();
		if(comando == "encenderya")  digitalWrite(relayCalefon, LOW);
		if(comando == "apagarya")  digitalWrite(relayCalefon, HIGH);
		if(comando == "activar temporizador")
		{
			 temporizadorActivado=true;
			 if(statusTemporizador=="apagar")
			 {
			 	digitalWrite(relayCalefon, HIGH);
			 }
		}  
		if(comando == "desactivar temporizador")
		{
			 temporizadorActivado=false;
			 digitalWrite(relayCalefon, LOW);
		}  
		if(comando == "encender")
		{
			 statusTemporizador="encender";
			 digitalWrite(relayCalefon, LOW);
		} 
		if(comando == "apagar")
		{
			 statusTemporizador="apagar";
			 if(temporizadorActivado=true)
			 {
			 	digitalWrite(relayCalefon, HIGH);
			 }
		}
	}
}

String revisarTalkBack()
{
	String cmd = "AT+CIPSTART=\"TCP\",\"";
	cmd += IP;
	cmd += "\",80";
	Serial.println(cmd);

	delay(2000);
	if(Serial.find("Error"))
	{
		connectWiFi();
	}

	cmd = "GET /talkbacks/";
	cmd +=talkbackid;
	cmd +="/commands/execute?api_key=";
	cmd +=talkbackkey;
	cmd += "\r\n";
	Serial.print("AT+CIPSEND=");
	Serial.println(cmd.length());
	if(Serial.find(">"))
	{
		Serial.print(cmd);
	}
	else
	{
		Serial.println("AT+CIPCLOSE");
	}

	Serial.flush();

	String talkBackCommand;
	char charIn;
	while (Serial.available() > 0)
	{
		Serial.read();
	}
	for(int iteracion = 0; iteracion < 20; iteracion++)
	{
		while(Serial.available() > 0)
		{
			if(Serial.find("+IPD,") == true)
			{
				Serial.find(":");
				while (Serial.available() > 0)
				{
					charIn = Serial.read();
					if((String)charIn == "\r") return talkBackCommand;
					if((String)charIn == "\n") return talkBackCommand;
					talkBackCommand += charIn;
				}
				break;
			}
			else
			{
				while (Serial.available() > 0)
				{
					Serial.read();
				}
			}
		}
		delay(1000);
	}
	return talkBackCommand;
}

void subirConsumo(String consumoW, String consumoW24, String temperatura, String humedad)
{
	String cmd = "AT+CIPSTART=\"TCP\",\"";
	cmd += IP;
	cmd += "\",80";
	Serial.println(cmd);
	delay(2000);
	if(Serial.find("Error"))
	{
		connectWiFi();
		return;
	}
	GET += thingspeakkey;
	GET +="&";
	cmd = GET;
	cmd += "field1=";
	cmd += consumoW;
	cmd += "&field2=";
	cmd += consumoW24;
	cmd += "&field5=";
	cmd += temperatura;
	cmd += "&field6=";
	cmd += humedad;
	cmd += "&status=";
	if(temporizadorActivado) cmd += "temporizador_activado";
	if(temporizadorActivado==false) cmd += "temporizador_desactivado";
	cmd += "/";
	cmd += statusTemporizador;
	cmd += "\r\n";
	Serial.print("AT+CIPSEND=");
	Serial.println(cmd.length());
	if(Serial.find(">"))
	{
		Serial.print(cmd);
	}
	else
	{
		Serial.println("AT+CIPCLOSE");
	}
}

float readCurrent(int PIN)
{
	unsigned long currentAcc = 0;
	unsigned int count = 0;
	unsigned long prevMicros = micros() - sampleInterval ;
	long media = 0;
	while (count < numSamples)
	{
		if (micros() - prevMicros >= sampleInterval)
		{

			long adc_raw = analogRead(PIN);
			adc_raw *= 10;
			media += adc_raw;
			adc_raw -= adc_zero;
			currentAcc += (unsigned long)(adc_raw * adc_raw);
			++count;
			prevMicros += sampleInterval;
		}
	}
	media /= numSamples;
	promedio2 += int(media);

	float rms = (sqrt((float)currentAcc / (float)numSamples) * (0.073982)) / 10; // de dónde sale 0.073982?
	return rms;
}

String convertirInt(int numero){
	char buffer[12];
	String tempF = itoa(numero, buffer, 10);
	return tempF;
}

String convertirFloat(float numero){
	int valorTemporal;
	String temp2;
	valorTemporal = int(numero);
	temp2= convertirInt(valorTemporal);
	return temp2;
}

boolean connectWiFi()
{
	Serial.println("AT+CWMODE=1");
	delay(2000);
	String cmd = "AT+CWJAP=\"";
	cmd += SSID;
	cmd += "\",\"";
	cmd += PASS;
	cmd += "\"";
	Serial.println(cmd);
	delay(5000);
	if(Serial.find("OK"))
	{
		return true;
	}
	else
	{
		return false;
	}
}
