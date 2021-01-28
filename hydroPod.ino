/*
  This example code is in the public domain.
*/

// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

using namespace std;

#include <Wire.h>
#include <SPI.h>

// Unified sensor class
#include <Adafruit_Sensor.h>

// DHT Driver for unified Sensor class
#include <DHT.h>
#include <DHT_U.h>

#include <Adafruit_BME280.h>

//#include "ListLib.h"

#define DHT1PIN   2

#define MAX_SENSORS  ((unsigned char)(8))
char sensor_index = 0;

#define MAX_SUBNAME 7
typedef struct sensor_list_s 
{
   Adafruit_Sensor *sensor;
   char subname[MAX_SUBNAME+1];
   uint32_t last_sample;
   uint32_t sample_period_MS; 
} sensor_list_t;

sensor_list_t sensors[MAX_SENSORS];

void add_sensor(Adafruit_Sensor *sensor, char const *subname)
{
  if (sensor_index == MAX_SENSORS)
    Serial.print(F("No space for sensor"));
  else
  {
    sensors[(short)sensor_index].sensor = sensor;
    strncpy(&sensors[(short)sensor_index].subname[0], subname, MAX_SUBNAME);
    sensors[(short)sensor_index].last_sample = 0;
    sensors[(short)sensor_index].sample_period_MS = 0;
    sensor_index++;
  }
}

void enroll_BME280(Adafruit_BME280 *bme)
{
  add_sensor(bme->getTemperatureSensor(),String(F("Temp")).c_str());
  add_sensor(bme->getPressureSensor(),   String(F("Pres")).c_str());
  add_sensor(bme->getHumiditySensor(),   String(F("Hum")).c_str());
}

void enroll_DHT(DHT_Unified *dht)
{
  add_sensor(dht->ptrTemperature(), String(F("Temp")).c_str());
  add_sensor(dht->ptrHumidity(),    String(F("Hum")).c_str());
}

char          UUID4_string[] = "63f59d39-e099-4704-8bde-7bdd58095631";
unsigned char UUID4_bytes[]  = {0x63, 0xf5, 0x9d, 0x39, 0xe0, 0x99, 0x47, 0x04, 0x8b, 0xde, 0x7b, 0xdd, 0x58, 0x09, 0x56, 0x31};

class MorseFlasher
{
  char          char_code[10];
  int           char_idx;
  char          msg_code[128];
  int           msg_idx;
  int           error_led;
  unsigned long error_element_end_time_ms;
  int           busy;

#define USE_SIXBIT 1
#if USE_SIXBIT
// Technically, this is the DEC 6-bit encoding as used in the 18 and 36-bit machines
#define SIXBIT(ch) ((ch - 0x20) & 0x3f)
#define ME(ch, sz, bits) (unsigned short)((SIXBIT(ch)<<10) | (sz<<7) | (bits << (7 - sz)))

  char *find_char_code(char ch)
  {
    static char char_code[8];
    int i;
    static const unsigned short morse[] =
    {
      ME( 'A', 2, 0b01 ),
      ME( 'B', 4, 0b1000 ),
      ME( 'C', 4, 0b1010 ),
      ME( 'D', 3, 0b100 ),
      ME( 'E', 1, 0b0 ),
      ME( 'F', 4, 0b0010 ),
      ME( 'G', 3, 0b110 ),
      ME( 'H', 4, 0b0000 ),
      ME( 'I', 2, 0b00 ),
      ME( 'J', 4, 0b0111 ),
      ME( 'K', 3, 0b101 ),
      ME( 'L', 4, 0b0100 ),
      ME( 'M', 2, 0b11 ),
      ME( 'N', 2, 0b10 ),
      ME( 'O', 3, 0b111 ),
      ME( 'P', 4, 0b0110 ),
      ME( 'Q', 4, 0b1101 ),
      ME( 'R', 3, 0b010 ),
      ME( 'S', 3, 0b000 ),
      ME( 'T', 1, 0b1 ),
      ME( 'U', 3, 0b001 ),
      ME( 'V', 4, 0b0001 ),
      ME( 'W', 3, 0b011 ),
      ME( 'X', 4, 0b1001 ),
      ME( 'Y', 4, 0b1011 ),
      ME( 'Z', 4, 0b1100 ),
      ME( '0', 5, 0b11111 ),
      ME( '1', 5, 0b01111 ),
      ME( '2', 5, 0b00111 ),
      ME( '3', 5, 0b00011 ),
      ME( '4', 5, 0b00001 ),
      ME( '5', 5, 0b00000 ),
      ME( '6', 5, 0b10000 ),
      ME( '7', 5, 0b11000 ),
      ME( '8', 5, 0b11100 ),
      ME( '9', 5, 0b11110 ),
      ME( '.', 6, 0b010101 ),
      ME( '?', 6, 0b001100 ),
      ME( ',', 6, 0b110011 ),
      ME( '/', 5, 0b10010 ),
      ME( '(', 5, 0b10110 ),
      ME( ')', 6, 0b101101 ),
      ME( '&', 5, 0b01000 ),
      ME( ':', 6, 0b111000 ),
      ME( ';', 6, 0b010101 ),
      ME( '\'',6, 0b011110 ),
      ME( '=', 5, 0b10001 ),
      ME( '+', 5, 0b01010 ),
      ME( '-', 6, 0b100001 ),
      ME( '_', 6, 0b001101 ),
      ME( '"', 6, 0b010010 ),
      ME( '$', 7, 0b0001001 ),
      ME( '@', 6, 0b011010 ),
      ME( '!', 6, 0b101011 ),
      0
    };
    if (islower(ch))
        ch = toupper(ch);
    for (i = 0;
         morse[i] != 0;
         i++)
    {
      if (((morse[i] >> 10) & 0x3f) == SIXBIT(ch))
      {
        unsigned char j = ((morse[i] >> 7) & 0x07);
        unsigned char k = 0;
        unsigned char bits = (morse[i]&0x007F);
        for ( ; j > 0; k++, j--, bits<<=1)
          char_code[k] = (((bits & 0x40) != 0) ? '=' : '.');
        char_code[k] = '\0';
//        Serial.print("Coded match: ");
//        Serial.print(morse[i], BIN);
//        Serial.print("  Char \"");
//        Serial.print(ch);
//        Serial.print("\" -> ");
//        Serial.println(char_code);
        return &char_code[0];
      }
    }
    Serial.print("Char \"");
    Serial.print(ch);
    Serial.println("\" -> not found");
    char_code[0] = ' ';
    char_code[1] = '\0';
    return &char_code[0];
  }
#else
  typedef const struct code_element_s
  {
    const char ch;
    const char elements[8];
  } code_element_t;

  // This representation of morse code is taking up almost 600 bytes, which is 30% of the total RAM on a Nano
  const char *find_char_code(char ch)
  {
    int i;
    static const code_element_t morse[] =
    {
      { 'a', ".=" },
      { 'b', "=..." },
      { 'c', "=.=." },
      { 'd', "=.." },
      { 'e', "." },
      { 'f', "..=." },
      { 'g', "==." },
      { 'h', "...." },
      { 'i', ".." },
      { 'j', ".===" },
      { 'k', "=.=" },
      { 'l', ".=.." },
      { 'm', "==" },
      { 'n', "=." },
      { 'o', "===" },
      { 'p', ".==." },
      { 'q', "==.=" },
      { 'r', ".=." },
      { 's', "..." },
      { 't', "=" },
      { 'u', "..=" },
      { 'v', "...=" },
      { 'w', ".==" },
      { 'x', "=..=" },
      { 'y', "=.==" },
      { 'z', "==.." },
      { '0', "=====" },
      { '1', ".====" },
      { '2', "..===" },
      { '3', "...==" },
      { '4', "....=" },
      { '5', "....." },
      { '6', "=...." },
      { '7', "==..." },
      { '8', "===.." },
      { '9', "====." },
      { ' ', " " },
      { '.', ".=.=.=" },
      { '?', "..==.." },
      { ',', "==..==" },
      { '/', "=..=." },
      { '(', "=.==." },
      { ')', "=-==.=" },
      { '&', ".=..." },
      { ':', "===..." },
      { ';', ".=.=.=" },
      { '=', "=...=" },
      { '+', ".=.=." },
      { '-', "=....=" },
      { '_', "..==.=" },
      { '"', ".=..=." },
      { '$', "...-..-" },
      { '@', ".==.=." },
      { '\0', "" }
    };
    if (isupper(ch))
      ch = tolower(ch);
    for (i = 0;
         morse[i].ch != '\0';
         i++)
    {
      if (morse[i].ch == ch)
        return &(morse[i].elements[0]);
    }
//    Serial.print(F("Char not found: "));
//    Serial.println(ch);
    return "..........";
  }
#endif
  
  long time_difference(unsigned long t1, unsigned long t2)
  {
    return (long)(t1-t2);
  }
  
  void set_LED(bool state)
  {
    if (state)
    {
      error_led = true;
      digitalWrite(LED_BUILTIN, 1);
    }
    else
    {
      error_led = false;
      digitalWrite(LED_BUILTIN, 0);
    }
  }
  
  int err_time_up(void)
  {
    return (time_difference(millis(), error_element_end_time_ms) >= 0);
  }
  
  void err_time_set(int quanta)
  {
    error_element_end_time_ms = millis() + (unsigned int)(75*quanta);
  }
  
  int blink_char_code(char new_letter)
  {
    if (!err_time_up())
      return false;
    if (!char_code[char_idx])
    {
      set_LED(false);
      err_time_set(3);
      if (new_letter != '\0')
      {
        strcpy(char_code, find_char_code(new_letter));
        char_idx = 0;
      }
      return true;
    }
    else if (char_code[char_idx] == ' ')
    {
      set_LED(false);
      err_time_set(9);
      char_idx++;
    }
    else if (char_code[char_idx] == '=')
    {
      if (error_led)
      {
        set_LED(false);
        err_time_set(1);
        char_idx++;
      }
      else
      {
        set_LED(true);
        err_time_set(3);
      }
    }
    else if (char_code[char_idx] == '.')
    {
      if (error_led)
      {
        set_LED(false);
        err_time_set(1);
        char_idx++;
      }
      else
      {
        set_LED(true);
        err_time_set(1);
      }
    }
    else
    {
      Serial.println("Bad char in char_code");
    }
    return false;
  }

public:

  MorseFlasher(void)
  {
    memset(char_code, 0, sizeof(char_code));
    char_idx = 0;
    memset(msg_code, 0, sizeof(msg_code));
    msg_idx = 0;
    error_led = 0;
    error_element_end_time_ms = millis();
    busy = 0;
  }

  void send(const char *msg)
  {
    Serial.print(" MF:\"");
    Serial.print(msg);
    Serial.print("\" ");
    if (busy)
        return;
    strncpy(msg_code, msg, sizeof(msg_code)-1);
    msg_idx = 0;
    error_led = 0;
    memset(char_code, 0, sizeof(char_code));
    char_idx = 0;
    err_time_set(1);
    busy = 1;
  }

  char const * get_msg(void)
  {
    return &msg_code[msg_idx];
  }
  
  void loop()
  {
    if (!err_time_up())
    {
      return;
    }
    if (!msg_code[msg_idx] && !char_code[char_idx])
    {
      busy = 0;
      err_time_set(1);
    }
    else if (blink_char_code(msg_code[msg_idx]))
    {
      if (!msg_code[msg_idx])
      {
        err_time_set(1);
      }
      else
      {
        err_time_set(3);
        msg_idx++;
      }
    }
    else
    {
    }
  }
};

MorseFlasher MF;



class PODtoPI
{
  char   *buffer;
  size_t  buffer_idx;
  size_t  send_idx;
  int     sending;
  size_t  buffer_limit;

public:

  // resets a message being constructed, or a message being transmitted.
  // used for error recovery, and when a message if finished transmitting.
  void  reset(void)
  {
    buffer_idx = 0;
    send_idx = 0;
    sending = false;
  }
  
  void send_poll(void)
  {
    size_t space;
    if (!sending)
      return;
    if (send_idx > buffer_idx)
    {
      // should not happen, but fix it if it is
      MF.send("send gt end" );
      reset();
      return;
    }
    space = Serial.availableForWrite();
    if (space > 0)
    {
      space = min(space, (buffer_idx - send_idx));
      Serial.write(&buffer[send_idx], space);
      send_idx += space;
      if (buffer_idx == send_idx)
      {
        // done sending the message.  It is still being sent
        // asynchronously, but all bytes have been moved out
        // of the buffer
        reset();
      }
    }
  }

private:

  bool resize_buffer(size_t new_size)
  {
    char *temp;
    Serial.print(" RSZ ");
    temp = (char *)malloc(new_size);
    if (!temp)
    {
      Serial.print(" NoRAM ");
      MF.send("No RAM");
      reset();
      return false;
    }
    if (buffer)
    {
      memcpy(temp, buffer, buffer_idx);
    }
    buffer_limit = new_size;
    buffer = temp;
    return true;
  }

  void add_poll(void)
  {
    while (sending)
    {
      if (send_idx != 0)
      {
        send_poll();
        delay(10);
      }
    }
  }

  
public:

  PODtoPI(void)
  {
    buffer = NULL;
    buffer_limit = 0;
    buffer_idx = 0;
    send_idx = 0;
    sending = false;
  }

  void release(void)
  {
    if (buffer)
    {
      free(buffer);
      buffer = NULL;
      buffer_limit = 0;
      buffer_idx = 0;
      send_idx = 0;
      sending = false;
    }
  }
  
  void  add(size_t size, unsigned char *b)
  {
    add_poll();
    if (buffer_idx+size > buffer_limit)
    {
      if (!resize_buffer(buffer_idx+size+128))
      {
        return;      
      }
    }
    memcpy(&buffer[buffer_idx], b, size);
    buffer_idx += size;         
    Serial.print(" +");
    Serial.print(size);
    Serial.print("=");
    Serial.print(buffer_idx);
    Serial.print(" ");
  }

  void  add(unsigned char b)
  {
    add_poll();
    if (buffer_idx+1 > buffer_limit)
    {
      if (!resize_buffer(buffer_idx+1+128))
      {
        return;
      }
    }
    buffer[buffer_idx++] = b;
    Serial.print(" +1");
    Serial.print("=");
    Serial.print(buffer_idx);
    Serial.print(" ");
  }
  
  void  add(unsigned short h)
  {
    add_poll();
    if (buffer_idx+2 > buffer_limit)
    {
      if (resize_buffer(buffer_idx+2+128))
      {
        return;
      }
    }
    buffer[buffer_idx++] = h&0xff;
    buffer[buffer_idx++] = (h>>8)&0xff;
    Serial.print(" +2");
    Serial.print("=");
    Serial.print(buffer_idx);
    Serial.print(" ");
  }

  void add_header(byte msg_type)
  {
    add_poll();
    if (buffer_idx != 0)
    {
      MF.send("err 1");
      reset();
    }
    add(msg_type);
    add((unsigned char)0);
    add((unsigned short)0);
    add((unsigned short)0);
    add((unsigned char)0);
    if (buffer_idx != 7)
    {
      Serial.print(" bx=");
      Serial.print(buffer_idx);
      Serial.print(" ");
      MF.send("?");
    }
  }

  void  send(int sync)
  {
    // sync -> wait until msg is sent.
    if (!sending)
    {
      // send the message.  First indicate that we are sending
      sending = true;
      // compute and fill in the header
      if (buffer_idx < 7)
      {
        MF.send("err 2");
        reset();
        return;
      }
      buffer[1] = ~buffer[0];
      buffer[4] = ~(buffer[2] = (buffer_idx&0xff));
      buffer[5] = ~(buffer[3] = ((buffer_idx>>8)&0xff));
      buffer[6] = 0;
      {
        unsigned char chksum = 0;
        for (size_t i = 0; i < buffer_idx; i++)
          chksum += buffer[i];
        buffer[6] = -chksum;
      }
      // send the first bytes, perhaps all the bytes
      send_poll();
      if (sync)
      {
        while (sending)
        {
          delay(10);
          send_poll();
        }
      }
    }
    else
    {
      MF.send("send busy ");
      reset();
    }
  }

  bool  empty_P()
  {
    return (buffer_idx == 0);
  }
};


PODtoPI msg_up;

// Response messages 
void send_device_description(int in_msg_size, unsigned char *in_message)
{
  //NOTUSED: in_msg_size, in_message
  // reply does not relate to any information in the message
  (void)in_msg_size;
  (void)in_message;
  msg_up.add_header(1);
  msg_up.add(16, &UUID4_bytes[0]);
  msg_up.add((unsigned char)1);
  msg_up.add((unsigned char)0); // no sub devices yet
  msg_up.send(0); // async send  
}

void send_ping_response(int in_msg_size, unsigned char *in_message)
{
  Serial.print(" imsz:");
  Serial.print(in_msg_size);
  Serial.print(" ");
  msg_up.add_header(3);
  msg_up.add(in_msg_size-7, &in_message[7]);
  msg_up.send(0);
}

void set_device_parameters(int in_msg_size, unsigned char *in_message)
{
  
}


class RM
{
  static const int RM_MAX_MSG = 128;
  MorseFlasher& MF;
  typedef enum 
  {
    RM_IDLE,
    RM_GOT_TYPE,
    RM_CONF_TYPE,
    RM_MSGSIZE_1,
    RM_MSGSIZE_2,
    RM_CONF_MSGSIZE_1,
//    RM_CONF_MSGSIZE_2,
    RM_READ_MSG
  } RM_STATES;
  
  RM_STATES     state;
  int           sub_state;
  int           msg_index;
  unsigned char message[RM_MAX_MSG+1];
  int           msg_size;
  unsigned long last_byte_received;

  long time_difference(unsigned long t1, unsigned long t2)
  {
    return (long)(t1-t2);
  }
 
public:
  RM(MorseFlasher& _MF) : MF(_MF)
  {
    state = RM_IDLE;
    sub_state = 0;
    memset(message, 0, sizeof(message));
    msg_index = 0;
    msg_size = 0; 
    last_byte_received = millis();
  }

  void time_check(void)
  {
    if ((state != RM_IDLE) &&
        (time_difference(millis(),last_byte_received) > 1000))
    {
      MF.send("to");
      msg_index = 0;
      state = RM_IDLE;
    }
  }

  void receive_byte(unsigned char b)
  {
    last_byte_received = millis();
    if (msg_index > RM_MAX_MSG)
    {
      MF.send("too big err ");
  error_return:
  completion_return:
      state = RM_IDLE;
      sub_state = 0;
      memset(message, 0, sizeof(message));
      msg_index = 0;
      msg_size = 0;
      return;
    }
    message[msg_index++] = b;
    switch (state)
    {
      case RM_IDLE:
        if (msg_index != 1)
        {
          MF.send("int err");
          goto error_return;
        }
        state = RM_GOT_TYPE;
        break;
      case RM_GOT_TYPE:
        if (message[0] != (0xff^message[1]))
        {
          MF.send("type err");
          goto error_return;
        }
        state = RM_CONF_TYPE;
        break;
      case RM_CONF_TYPE:
        state = RM_MSGSIZE_1;
        break;
      case RM_MSGSIZE_1:
        state = RM_MSGSIZE_2;
        break;
      case RM_MSGSIZE_2:
        if (message[2] != (0xff^message[4]))
        {
          MF.send("se1");
          goto error_return;
        }
        state = RM_CONF_MSGSIZE_1;
        break;
      case RM_CONF_MSGSIZE_1:
        if (message[3] != (0xff^message[5]))
        {
          MF.send("se2");
          goto error_return;
        }
        msg_size = ((message[3]<<8) | message[2]);
        if (msg_size < msg_index)
        {
          MF.send("se3");
          goto error_return;
        }
        state = RM_READ_MSG;
        break;
      case RM_READ_MSG:
        if (msg_size > msg_index)
        {
          // stay in this state and accumulate bytes, unless a timeout happens
          break;
        }
        else if (msg_size < msg_index)
        {
          MF.send("sz?");
          goto error_return;
        }
        else
        {
          unsigned char chksum = 0;
          // message is complete.  Check the checksum.
          for (int i=0, chksum=0; i<msg_size; i++)
            chksum += message[i];
          if (chksum != 0)
          {
            MF.send("chk");
            goto error_return;
          }
          // message is good.  Go handle it.
          switch (message[0]) // type
          {
            case 1:   // send device description
              send_device_description(msg_size, message);
              break;
            case 2:   // send pink response
              send_ping_response(msg_size, message);
              break;
            case 3:   // set parameters
              set_device_parameters(msg_size, message);
              break;
            default:
              MF.send("bad msg type");
              goto error_return;
          }
        }
        goto completion_return;
      default:
        MF.send("bad state   ");
        goto error_return;
    }
  }
};

RM com(MF);

Adafruit_BME280 bme;
DHT_Unified dht1(DHT1PIN, DHT22);

void setup() 
{
  memset(&sensors[0], 0, sizeof(sensors));
  // initialize serial:
  Serial.begin(9600);
  // initialize the BME sensor
  bme.begin(BME280_ADDRESS_ALTERNATE);
  enroll_BME280(&bme);
  // initialize the DHT21 sensor
  dht1.begin();
  enroll_DHT(&dht1);
  // make the pins outputs:
  pinMode(LED_BUILTIN, OUTPUT);
  MF.send("ok");
  Serial.print  (F("Sensor count = ")); Serial.println(sensor_index);
  sensor_t sensor;
  Serial.println(F("------------------------------------"));
  for (int i = 0; i < sensor_index; i++)
  {
    sensors[i].sensor->getSensor(&sensor);
    Serial.print  (F("Sensor #"));      Serial.println(i);
    sensors[i].sensor->printSensorDetails();
    Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
    Serial.print  (F("  SubSensor: ")); Serial.println(sensors[i].subname);
//    Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
//    Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
//    Serial.print  (F("Max Value:   ")); Serial.println(sensor.max_value); 
//    Serial.print  (F("Min Value:   ")); Serial.println(sensor.min_value); 
//    Serial.print  (F("Resolution:  ")); Serial.println(sensor.resolution); 
    sensors[i].sample_period_MS = sensor.min_delay/1000;
//    Serial.print  (F("  Min Delay: ")); Serial.println(sensors[i].sample_period_MS);
    Serial.println(F("------------------------------------"));
  }  
}

void loop() 
{
  sensors_event_t event;
  int i;
  for (i=0; i<sensor_index; i++)
  {
    if ((sensors[i].last_sample + sensors[i].sample_period_MS) < millis())
    {
      // get the data now
      int float_count = 0;
      sensors[i].sensor->getEvent(&event);
      sensors[i].last_sample = millis()+1;
      Serial.print(i); Serial.print(": ");
      switch (event.type) 
      {
        case SENSOR_TYPE_ACCELEROMETER:
        case SENSOR_TYPE_MAGNETIC_FIELD:
        case SENSOR_TYPE_ORIENTATION:
        case SENSOR_TYPE_GYROSCOPE:
        case SENSOR_TYPE_GRAVITY:
        case SENSOR_TYPE_ROTATION_VECTOR:
        case SENSOR_TYPE_LINEAR_ACCELERATION:
        case SENSOR_TYPE_COLOR:
          float_count = 3;
          Serial.print("F1: ");       Serial.print  (event.data[0]);
          Serial.print("  F2: ");     Serial.print  (event.data[1]);
          Serial.print("  F3: ");     Serial.println(event.data[2]);
          break;
        case SENSOR_TYPE_LIGHT:
        case SENSOR_TYPE_PRESSURE:
        case SENSOR_TYPE_PROXIMITY:
        case SENSOR_TYPE_CURRENT:
        case SENSOR_TYPE_VOLTAGE:
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        case SENSOR_TYPE_OBJECT_TEMPERATURE:
          float_count = 1;
          Serial.print("F:  ");     Serial.println(event.data[0]);
          break;
      }
    }
  }
  while (Serial.available() > 0) 
  {
    com.receive_byte(Serial.read());
  }
  com.time_check();    // handles timeouts in received messages
  MF.loop();           // handles Morse code blinking
  msg_up.send_poll();  // keeps message flowing out
}
