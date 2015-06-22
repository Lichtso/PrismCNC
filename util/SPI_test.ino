void setup() {
  DDRB &= ~((1<<2)|(1<<3)|(1<<5));   // SCK, MOSI and SS as inputs
  DDRB |= (1<<4);                    // MISO as output

  SPCR &= ~(1<<MSTR);                // Set as slave 
  //SPCR |= (1<<SPR0)|(1<<SPR1);       // divide clock by 128
  SPCR |= (1<<SPE);                  // Enable SPI
  
  Serial.begin(9600);
  Serial.println("Ready");
}

void loop() {
  while(!(SPSR & (1<<SPIF)));
  Serial.print("Received: ");
  Serial.println(SPDR);
}
