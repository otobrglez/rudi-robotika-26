// Blink: utripanje vgrajene LED diode (pin 13 na Uno/Nano)

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("LED prizgana");
  delay(1000);

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("LED ugasnjena");
  delay(1000);
}
