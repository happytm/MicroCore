/*
  ATtiny13 internal oscillator tuner

  By Ralph Doncaster (2019)
  Tweaked and tuned by MCUdude
  ------------------------------------------------------------------------------

  [ See diagram: https://github.com/MCUdude/MicroCore#minimal-setup ]

  Tunes the internal oscillator using a software serial implementation

  Start off by opening the serial monitor and select the correct baud rate.
  Make sure you're not sending any line ending characters (CR, LF).
  Repedeatly press 'x' [send] to tune the internal oscillator. After a few
  messages you'll eventually see readable text in the serial monitor, and a
  new, stable OSCCAL value. Remember to store this value to EEPROM (or hard-code
  it in your code) in order to have an accurate oscillator in future projects.

  RECOMMENDED SETTINGS FOR THIS SKETCH
  ------------------------------------------------------------------------------

  Tools > Board          : ATtiny13
  Tools > BOD            : [Use any BOD level you like]
  Tools > Compiler LTO   : LTO enabled
  Tools > Clock          : 9.6 MHz or 4.8 MHz depending on what osc. to tune
  Tools > Timing         : Millis disabled, Micros disabled

  SERIAL REMINDER
  ------------------------------------------------------------------------------
  The baud rate is IGNORED on the ATtiny13 due to using a simplified serial.
  The actual Baud Rate used is dependant on the processor speed.
  Note that you can specify a custom baud rate if the following ones does
  not fit your application.

  THESE CLOCKS USES 115200 BAUD:   THIS CLOCK USES 57600 BAUD:
  (External)  20 MHz               (Internal) 4.8 MHz
  (External)  16 MHz
  (External)  12 MHz
  (External)   8 MHz
  (Internal) 9.6 MHz

  THESE CLOCKS USES 19200 BAUD:    THIS CLOCK USES 9600 BAUD:
  (Internal) 1.2 MHz               (Internal) 600 KHz
  (External)   1 MHz

  If you get garbage output:
   1. Check baud rate as above
   2. Check if you have anything else connected to TX/RX like an LED
   3. You haven't sent enough 'x' characters yet
*/

const int8_t delta_val = 8;
const uint8_t uart_rx_pin = 1;
uint8_t oldOSCCAL;

// Converts 4-bit nibble to ascii hex
uint8_t nibbletohex(uint8_t value)
{
  value &= 0x0F;
  if (value > 9)
    value += 'A' - ':';
  return value + '0';
}

// Function to run when Rx interrupt occurs
void rxInterrupt()
{
  // Start timer when pin transitions low
  if ((PINB & _BV(uart_rx_pin)) == 0)
    TCCR0B = _BV(CS00);
  else
  {
    uint8_t current = TCNT0;
    // End of interval, reset counter
    TCCR0B = 0;
    TCNT0 = 0;
    // 'x' begins with 3 zeros + start bit = 4
    uint8_t target = (uint8_t)(4 * F_CPU / BAUD_RATE);
    int8_t delta = target - current;
    if (delta > delta_val)  OSCCAL++;
    if (delta < -delta_val) OSCCAL--;
    asm("lpm"); // 3 cycle delay
    // Print calculation
    Serial.print(F("Delta: 0x"));
    Serial.write(nibbletohex(delta >> 4));
    Serial.write(nibbletohex(delta));
    Serial.print(F("  New cal: 0x"));
    Serial.write(nibbletohex(OSCCAL >> 4));
    Serial.write(nibbletohex(OSCCAL));
    Serial.write('\n');
  }
 
  // Clear interrupt flag in case another triggered
  GIFR = _BV(INTF0);
}

void setup() 
{
  // Note that any baud rate specified is ignored on the ATtiny13. See header above.
  Serial.begin();
  
  // Store old OSCCAL value for reference
  oldOSCCAL = OSCCAL;
  
  // Prepare for sleep mode
  MCUCR = _BV(SE);

  // Enable Rx pin pullup
  digitalWrite(uart_rx_pin,HIGH);
  
  // Setup RX interrupt
  attachInterrupt(0, rxInterrupt, CHANGE);
  
  delay(1000);

  // Print default message
  Serial.write('x');
  Serial.write('\n');
  
  // Wait for tuning character to ensure not reading noise
  // before entering tuning mode
  wait_x:
  uint8_t counter = 0;
  while (digitalRead(uart_rx_pin));
  do 
  {
    counter++;
  } while (digitalRead(uart_rx_pin) == 0);

  // Low period should be 4 bit-times
  uint8_t margin = BIT_CYCLES/8;
  if (counter - (uint8_t)BIT_CYCLES > margin)
    goto wait_x;
  else
  {
    if ((uint8_t)BIT_CYCLES - counter > margin)
      goto wait_x;
  }

  delay(1); // Skip remaining bits in frame
}

void loop() 
{
  // Nothing here really...
  asm("sleep");
}