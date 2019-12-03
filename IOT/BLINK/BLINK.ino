// Initial Setup
void setup() 
{
  // Set pin as OUTPUT
  pinMode(LED_BUILTIN, OUTPUT); 
}

// Program Loop
void loop() 
{
  // Turn LED on and wait 1000 ms (1 sec)
  digitalWrite(LED_BUILTIN, HIGH); 
  delay(1000); 

  // Turn LED off and wait 1000 ms (1 sec)
  digitalWrite(LED_BUILTIN, LOW); 
  delay(1000); 
}
