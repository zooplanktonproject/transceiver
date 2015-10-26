#define NUM_NODES     100  // maximum LED node number we can receive from Raspberry Pi
#define SPIRE_MAXLEN  60   // maximum message length we can relay to the spire controller

#define NODES_SERIAL1  45
#define NODES_SERIAL2  45

// incoming data
unsigned char incoming_nodes[NUM_NODES*3];
char incoming_spire_message[SPIRE_MAXLEN];
int incoming_state = 0; // 0=none, 1=nodes, 2=spire
int incoming_index = 0;
int incoming_led;
int incoming_red;
int incoming_green;
int incoming_blue;

// queued for next transmit
unsigned char queue_serial1[NODES_SERIAL1 * 3];
unsigned char queue_serial2[NODES_SERIAL2 * 3];
unsigned char queue_serial3[SPIRE_MAXLEN];
int queue_serial1_hasdata = 0;
int queue_serial2_hasdata = 0; // zero means the queue is empty
int queue_serial3_length = 0;

// transmitting data
unsigned char transmit_serial1[NODES_SERIAL1 * 3];
unsigned char transmit_serial2[NODES_SERIAL2 * 3];
unsigned char transmit_serial3[SPIRE_MAXLEN];
int transmit_serial1_active = 0;
int transmit_serial2_active = 0; // zero means transmitter is idle
int transmit_serial3_active = 0;
int transmit_serial1_index = 0;
int transmit_serial2_index = 0;
int transmit_serial3_index = 0;
int transmit_serial3_length = 0;

// remember how much each serial port can buffer
// so we can later detect when it's fully transmitted
int serial1_buffer_length;
int serial2_buffer_length;
int serial3_buffer_length;

void setup()
{
  startupBlink();

  Serial1.begin(115200);
  Serial2.begin(115200);
  Serial3.begin(115200);
  serial1_buffer_length = Serial1.availableForWrite();
  serial2_buffer_length = Serial2.availableForWrite();
  serial3_buffer_length = Serial3.availableForWrite();
}

// copy data from incoming_nodes[] to queue_serial1[]
// this code determines which LEDs are mapped to Serial1
void queue_nodes_serial1(void)
{
  int begin_led = 1;

  for (int i=0; i<NODES_SERIAL1; i++) {
    queue_serial1[i*3+0] = incoming_nodes[(begin_led+i)*3+0];
    queue_serial1[i*3+1] = incoming_nodes[(begin_led+i)*3+1];
    queue_serial1[i*3+2] = incoming_nodes[(begin_led+i)*3+2];
  }
}

// copy data from incoming_nodes[] to queue_serial2[]
// this code determines which LEDs are mapped to Serial2
void queue_nodes_serial2(void)
{
  int begin_led = 46;

  for (int i=0; i<NODES_SERIAL2; i++) {
    queue_serial2[i*3+0] = incoming_nodes[(begin_led+i)*3+0];
    queue_serial2[i*3+1] = incoming_nodes[(begin_led+i)*3+1];
    queue_serial2[i*3+2] = incoming_nodes[(begin_led+i)*3+2];
  }
}

void loop()
{
  // Receive any incoming data from the Raspberry Pi
  int avail = Serial.available();
  while (avail > 0) {
    unsigned char c = Serial.read();
    if (c == '@') {
      // begin RGB LED node data
      incoming_state = 1;
      incoming_index = 0;
    } else if (c == '#') {
      // end RGB LED node data
      if (incoming_state == 1) {
        queue_nodes_serial1();
        queue_serial1_hasdata = 1;
        queue_nodes_serial2();
        queue_serial2_hasdata = 1;
      }
      incoming_state = 0;
    } else if (c == '$') {
      // begin spire message
      incoming_state = 2;
      incoming_index = 0;
      incoming_spire_message[incoming_index] = c;
      incoming_index++;
    } else if (c == '%') {
      // end spire message
      if (incoming_state == 2 && incoming_index > 0) {
        memcpy(queue_serial3, incoming_spire_message, incoming_index);
        queue_serial3_length = incoming_index;
      }
      incoming_state = 0;
    } else {
      if (incoming_state == 1) {
        // decode incoming LED data as it arrives
        if (c >= '0' && c <= '9') {
          int n = c - '0';
          switch (incoming_index) {
          case  0: incoming_led   = n * 100; break;
          case  1: incoming_led   += n * 10; break;
          case  2: incoming_led   += n;      break;
          case  3: incoming_red   = n * 100; break;
          case  4: incoming_red   += n * 10; break;
          case  5: incoming_red   += n;      break;
          case  6: incoming_green = n * 100; break;
          case  7: incoming_green += n * 10; break;
          case  8: incoming_green += n;      break;
          case  9: incoming_blue  = n * 100; break;
          case 10: incoming_blue  += n * 10; break;
          case 11: incoming_blue  += n;      break;
          default: incoming_index = 0;  break;
          }
          incoming_index++;
          if (incoming_index >= 12) {
            if (incoming_led < NUM_NODES) {
              n = incoming_led * 3;
              incoming_nodes[n+0] = incoming_red;
              incoming_nodes[n+1] = incoming_green;
              incoming_nodes[n+2] = incoming_blue;
            }
            incoming_index = 0;
          }
        }
      } else if (incoming_state == 2) {
        // simply store the spire message
        if (incoming_index < SPIRE_MAXLEN) {
          incoming_spire_message[incoming_index] = c;
          incoming_index++;
        }
      } else {
        // ignore anything unknown
      }
    }
    avail = avail - 1;
  }

  // Check each queue, and begin transmitting if possible
  if (queue_serial1_hasdata > 0 && transmit_serial1_active == 0) {
    // begin transmitting on Serial1
    memcpy(transmit_serial1, queue_serial1, NODES_SERIAL1*3);
    transmit_serial1_index = 0;
    queue_serial1_hasdata = 0;
    Serial1.begin(38400);
    Serial1.write(0); // start of frame message
    Serial1.flush();
    Serial1.begin(115200);
    Serial1.write(0);
    transmit_serial1_active = 1;
  }
  if (queue_serial2_hasdata > 0 && transmit_serial2_active == 0) {
    // begin transmitting on Serial2
    memcpy(transmit_serial2, queue_serial2, NODES_SERIAL2*3);
    transmit_serial2_index = 0;
    queue_serial2_hasdata = 0;
    Serial2.begin(38400);
    Serial2.write(0); // start of frame message
    Serial2.flush();
    Serial2.begin(115200);
    Serial2.write(0);
    transmit_serial2_active = 1;
  }
  if (queue_serial3_length > 0 && transmit_serial3_active == 0) {
    // begin transmitting on Serial3
    memcpy(transmit_serial3, queue_serial3, queue_serial3_length);
    transmit_serial3_index = 0;
    transmit_serial3_length = queue_serial3_length;
    queue_serial3_length = 0;
    transmit_serial3_active = 1;
  }

  // Continue transmitting....
  if (transmit_serial1_active) {
    int space = Serial1.availableForWrite();
    if (space > 0) {
      if (transmit_serial1_index < NODES_SERIAL1*3) {
        // we still need to transmit data
        int n = NODES_SERIAL1*3 - transmit_serial1_index;
        if (n > space) n = space;
        Serial1.write(transmit_serial1 + transmit_serial1_index, n);
        transmit_serial1_index += n;
      } else {
        // everything transmitted, but still waiting
        // for Serial1 to actually send it...
        if (space >= serial1_buffer_length) {
          // ok, looks like it's all left the building...
          // but do a flush, just to be sure!
          Serial1.flush();
          transmit_serial1_active = 0;
          transmit_serial1_index = 0;
        }
      }
    }
  }
  if (transmit_serial2_active) {
    int space = Serial2.availableForWrite();
    if (space > 0) {
      if (transmit_serial2_index < NODES_SERIAL2*3) {
        // we still need to transmit data
        int n = NODES_SERIAL2*3 - transmit_serial2_index;
        if (n > space) n = space;
        Serial2.write(transmit_serial2 + transmit_serial2_index, n);
        transmit_serial2_index += n;
      } else {
        // everything transmitted, but still waiting
        // for Serial2 to actually send it...
        if (space >= serial2_buffer_length) {
          // ok, looks like it's all left the building...
          // but do a flush, just to be sure!
          Serial2.flush();
          transmit_serial2_active = 0;
          transmit_serial2_index = 0;
        }
      }
    }
  }
  if (transmit_serial3_active) {
    int space = Serial3.availableForWrite();
    if (space > 0) {
      if (transmit_serial3_index < transmit_serial3_length) {
        // we still need to transmit data
        int n = transmit_serial3_length - transmit_serial3_index;
        if (n > space) n = space;
        Serial3.write(transmit_serial3 + transmit_serial3_index, n);
        transmit_serial3_index += n;
      } else {
        // everything transmitted, but still waiting
        // for Serial3 to actually send it...
        if (space >= serial3_buffer_length) {
          // ok, looks like it's all left the building...
          // but do a flush, just to be sure!
          Serial3.flush();
          transmit_serial3_active = 0;
          transmit_serial3_index = 0;
          transmit_serial3_length = 0;
        }
      }
    }
  }

  // TODO: timeout?  maybe automatically play animations
  //  if the Raspberry Pi doesn't send data??

}

void startupBlink() {
  // turn on the LED
  pinMode(13, OUTPUT);

  for(int x = 0; x < 4; x++) {
    digitalWrite(13, HIGH);
    delayMicroseconds(100000);
    digitalWrite(13, LOW);
    delayMicroseconds(100000);
  }

  digitalWrite(13, HIGH);
}
