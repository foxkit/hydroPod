/*
  This example code is in the public domain.
*/


char          UUID4_string[] = "63f59d39-e099-4704-8bde-7bdd58095631";
unsigned char UUID4_bytes[]  = {0x63, 0xf5, 0x9d, 0x39, 0xe0, 0x99, 0x47, 0x04, 0x8b, 0xde, 0x7b, 0xdd, 0x58, 0x09, 0x56, 0x31};

int flasher_count = 0;

class Sensor
{
  typedef enum 
  { 
    SENSOR_I2c,
    SENSOR_GPIO,
    SENSOR_ONE_WIRE,
    SENSOR_ANALOG,
    SENSOR_UNKNOWN
  } SENSOR_t;

  Sensor *next = NULL;
  SENSOR_t type;
  unsigned char id;  // sensor ID
  bool updated;      // true if this sensor has been updated since it was transmitted
  union
  {
    struct
    {
      unsigned char address;
    } i2c;
    struct
    {
      int pin;
      int polarity;
    } gpio;
    struct
    {
      int pin;
      int sequence; // first, second, third -- based on discovery order
    } one_wire;
    struct
    {
      int pin;
      int d_min;
      int d_max;
      float v_min;
      float v_max;
    } analog;
  } u;

public:
  Sensor(SENSOR_t _type, unsigned char _id, Sensor *_next)
  {
    type    = _type;
    next    = _next;
    id      = _id;
    updated = false;
  }
};

Sensor *sensor_list = NULL;



class MorseFlasher
{
  char          char_code[10];
  int           char_idx;
  char          msg_code[128];
  int           msg_idx;
  int           error_led;
  unsigned long error_element_end_time_ms;
  int           busy;
  
  typedef const struct code_element_s
  {
    const char ch;
    const char elements[8];
  } code_element_t;
  
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
//    Serial.print("Char not found: ");
//    Serial.println(ch);
    return "........";
  }
  
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
//    Serial.print("c");
//    Serial.print(msg_index-1);
//    Serial.print(":");
//    Serial.print(state);
//    Serial.print("=");
//    Serial.print(b,HEX);
//    Serial.print("  ");
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

void setup() 
{
  // initialize serial:
  Serial.begin(9600);
  // make the pins outputs:
  pinMode(LED_BUILTIN, OUTPUT);
  MF.send("ok");
}


void loop() 
{
  // if there's any serial available, read it:
  while (Serial.available() > 0) 
  {
    com.receive_byte(Serial.read());
  }
  com.time_check();    // handles timeouts in received messages
  MF.loop();           // handles Morse code blinking
  msg_up.send_poll();  // keeps message flowing out
}
