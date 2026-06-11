#include <BluetoothSerial.h>
#include <TinyGPS++.h>
#include <ESP32Servo.h>

// ── Pinos ──────────────────────────────────────────
#define TRIG_PIN    5
#define ECHO_PIN    18
#define LED_VERDE   25
#define LED_AMARELO 26
#define LED_VERM    27
#define SERVO_PIN   19
#define GPS_RX      16 
#define GPS_TX      17

// ── Calibração da lixeira (em cm) ──────────────────
#define DIST_VAZIA  25.5
#define DIST_CHEIA   5.0

// ── Objetos ────────────────────────────────────────
BluetoothSerial SerialBT;
TinyGPSPlus    gps;
HardwareSerial gpsSerial(2);   // UART2
Servo          servoTrava;

// ── Estado ─────────────────────────────────────────
unsigned long ultimoPisca   = 0;
unsigned long ultimoGPS     = 0;
bool          ledLigado      = false;
bool          travaFechada   = false;
int           pinLedAtivo    = LED_VERDE;

// ──────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  SerialBT.begin("Lixeira_Smart");          // nome visível no BT
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  pinMode(TRIG_PIN,    OUTPUT);
  pinMode(ECHO_PIN,    INPUT);
  pinMode(LED_VERDE,   OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERM,    OUTPUT);

  servoTrava.attach(SERVO_PIN);
  servoTrava.write(25);   // inicia travado
  travaFechada = true;

  Serial.println("Sistema iniciado!");
}

// ── Mede distância (cm) ─────────────────────────────
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
  return duracao * 0.034 / 2.0;
}

// ── Calcula percentual de lixo ──────────────────────
int calcularPercentual(float dist) {
  if (dist <= DIST_CHEIA)  return 100;
  if (dist >= DIST_VAZIA)  return 0;
  return (int)(100.0 - ((dist - DIST_CHEIA) / (DIST_VAZIA - DIST_CHEIA)) * 100.0);
}

// ── Apaga todos os LEDs ─────────────────────────────
void apagarLeds() {
  digitalWrite(LED_VERDE,   LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERM,    LOW);
}

// ── Envia dados GPS + percentual via BT ─────────────
void enviarGPS(int percentual) {
  if (gps.location.isValid()) {
    String msg = "GPS - Lat: ";
    msg += String(gps.location.lat(), 6) + ", Long: ";
    msg += String(gps.location.lng(), 6) + ",";
    msg += "Percentual de lixo em:" + String(percentual) + "%";
    SerialBT.println(msg);
    Serial.println(msg);
  } else {
    SerialBT.println("GPS: aguardando sinal... LIXO:" + String(percentual) + "%");
  }
}

// ──────────────────────────────────────────────────
void loop() {

  // ── Lê GPS continuamente ────────────────────────
  while (gpsSerial.available()) {
  char c = gpsSerial.read();
  Serial.write(c);
  gps.encode(c);
}

  // ── Lê Bluetooth (comandos do app) ──────────────
  if (SerialBT.available()) {
    char cmd = (char)SerialBT.read();
    if (cmd == 'L') {
      servoTrava.write(25);
      travaFechada = true;
      SerialBT.println("TRAVA: fechada");
    } else if (cmd == 'U') {
      servoTrava.write(100);
      travaFechada = false;
      SerialBT.println("TRAVA: aberta");
    }
  }

  // ── Mede nível de lixo ──────────────────────────
  float dist       = medirDistancia();
  int   percentual = calcularPercentual(dist);

  Serial.print("Distancia: ");
  Serial.print(dist);
  Serial.print(" cm | Lixo: ");
  Serial.print(percentual);
  Serial.println("%");

  // ── Define LED ativo e estado do GPS ────────────
  bool gpsAtivo = false;

  if (percentual >= 85) {
    pinLedAtivo = LED_VERM;
    gpsAtivo    = true;
  } else if (percentual >= 60) {
    pinLedAtivo = LED_AMARELO;
    gpsAtivo    = true;
  } else {
    pinLedAtivo = LED_VERDE;
  }

  // ── Pisca LED a cada 10 segundos ────────────────
  unsigned long agora = millis();
  if (agora - ultimoPisca >= 10000) {
    ultimoPisca = agora;
    apagarLeds();
    ledLigado = !ledLigado;
    if (ledLigado) digitalWrite(pinLedAtivo, HIGH);
  }

  // ── Envia GPS a cada 5 segundos (se ativo) ──────
  if (gpsAtivo && (agora - ultimoGPS >= 5000)) {
    ultimoGPS = agora;
    enviarGPS(percentual);
  }

  delay(100);
}