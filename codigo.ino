#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ===== CONFIGURACAO WI-FI =====
const char* ssid = "SSID";
const char* password = "PASSWORD";

// ===== PINOS DOS SENSORES =====
const int TRIG_VAGA1 = 4;
const int ECHO_VAGA1 = 5;

const int TRIG_VAGA2 = 22;
const int ECHO_VAGA2 = 23;

const int TRIG_VAGA3 = 32;
const int ECHO_VAGA3 = 33;

// ===== PINOS DOS COMPONENTES =====
const int PINO_SERVO = 13;
const int LED_VERDE = 26;
const int LED_VERMELHO = 27;

// ===== CONFIGURACOES =====
const float DISTANCIA_OCUPADO_CM = 10.0;
const unsigned long INTERVALO_LEITURA = 300;

// Ajuste conforme a montagem mecanica da cancela.
const int ANGULO_FECHADO = 0;
const int ANGULO_ABERTO = 90;
const unsigned long TEMPO_CANCELA_ABERTA = 3000;

WebServer server(80);
Servo cancela;

bool vaga1Ocupada = false;
bool vaga2Ocupada = false;
bool vaga3Ocupada = false;

bool cancelaAberta = false;
unsigned long instanteAbertura = 0;
unsigned long ultimaLeitura = 0;

// Mede a distancia em centimetros.
// Retorna -1 se nao houver leitura valida.
float medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  unsigned long duracao = pulseIn(echo, HIGH, 30000);

  if (duracao == 0) {
    return -1;
  }

  return duracao * 0.0343 / 2.0;
}

bool detectarOcupacao(int trig, int echo) {
  float distancia = medirDistancia(trig, echo);

  // Sem leitura valida, nao considera a vaga livre.
  // Na montagem real, convem distinguir falha de sensor
  // de uma vaga efetivamente ocupada.
  if (distancia < 0) {
    return true;
  }

  return distancia <= DISTANCIA_OCUPADO_CM;
}

int vagasLivres() {
  return (!vaga1Ocupada) + (!vaga2Ocupada) + (!vaga3Ocupada);
}

void atualizarVagas() {
  vaga1Ocupada = detectarOcupacao(TRIG_VAGA1, ECHO_VAGA1);

  // Pequena pausa para reduzir interferencia entre sensores.
  delay(40);

  vaga2Ocupada = detectarOcupacao(TRIG_VAGA2, ECHO_VAGA2);

  // Pequena pausa para reduzir interferencia entre sensores.
  delay(40);

  vaga3Ocupada = detectarOcupacao(TRIG_VAGA3, ECHO_VAGA3);

  digitalWrite(
    LED_VERDE,
    vagasLivres() > 0 ? HIGH : LOW
  );

  digitalWrite(
    LED_VERMELHO,
    vagasLivres() == 0 ? HIGH : LOW
  );
}

void abrirCancela() {
  atualizarVagas();

  if (vagasLivres() > 0) {
    cancela.write(ANGULO_ABERTO);
    cancelaAberta = true;
    instanteAbertura = millis();
  }
}

void fecharCancela() {
  cancela.write(ANGULO_FECHADO);
  cancelaAberta = false;
}

String estadoVaga(bool ocupada) {
  if (ocupada) {
    return "Ocupada";
  }

  return "Livre";
}

void paginaPrincipal() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Estacionamento IoT</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 650px;
      margin: 30px auto;
      padding: 0 16px;
      background: #f3f4f6;
      color: #222;
    }
    .card {
      background: white;
      padding: 20px;
      margin: 12px 0;
      border-radius: 12px;
      box-shadow: 0 2px 8px #0001;
    }
    .livre { color: #16803c; }
    .ocupada { color: #c62828; }
    button {
      padding: 12px 18px;
      border: 0;
      border-radius: 8px;
      cursor: pointer;
      font-size: 16px;
      margin: 5px;
    }
    .abrir { background: #16803c; color: white; }
    .fechar { background: #333; color: white; }
  </style>
</head>
<body>
  <h1>Estacionamento Inteligente</h1>
  <p>Monitoramento de vagas com ESP32</p>

  <div class="card">
    <h2>Vaga 1</h2>
    <h3 id="vaga1">Carregando...</h3>
  </div>

  <div class="card">
    <h2>Vaga 2</h2>
    <h3 id="vaga2">Carregando...</h3>
  </div>

  <div class="card">
    <h2>Vaga 3</h2>
    <h3 id="vaga3">Carregando...</h3>
  </div>

  <div class="card">
    <h2>Resumo</h2>
    <p>Vagas livres: <strong id="livres">-</strong></p>
    <p>Vagas ocupadas: <strong id="ocupadas">-</strong></p>
    <p>Cancela: <strong id="cancela">-</strong></p>
  </div>

  <div class="card">
    <h2>Controle de acesso</h2>
    <button class="abrir" onclick="controlar('/abrir')">
      Abrir cancela
    </button>
    <button class="fechar" onclick="controlar('/fechar')">
      Fechar cancela
    </button>
    <p id="mensagem"></p>
  </div>

  <script>
    async function atualizar() {
      try {
        const resposta = await fetch('/estado');
        const dados = await resposta.json();

        ['vaga1', 'vaga2', 'vaga3'].forEach((id, i) => {
          const ocupada = dados[id];
          const elemento = document.getElementById(id);
          elemento.textContent = ocupada ? 'Ocupada' : 'Livre';
          elemento.className = ocupada ? 'ocupada' : 'livre';
        });

        document.getElementById('livres').textContent =
          dados.livres;

        document.getElementById('ocupadas').textContent =
          dados.ocupadas;

        document.getElementById('cancela').textContent =
          dados.cancela ? 'Aberta' : 'Fechada';
      } catch (erro) {
        document.getElementById('mensagem').textContent =
          'Falha na comunicacao com o ESP32.';
      }
    }

    async function controlar(rota) {
      try {
        const resposta = await fetch(rota);
        const resultado = await resposta.text();

        document.getElementById('mensagem').textContent =
          resultado;

        atualizar();
      } catch (erro) {
        document.getElementById('mensagem').textContent =
          'Nao foi possivel enviar o comando.';
      }
    }

    atualizar();
    setInterval(atualizar, 1000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=utf-8", html);
}

void enviarEstado() {
  String json = "{";
  json += "\"vaga1\":" + String(vaga1Ocupada ? "true" : "false") + ",";
  json += "\"vaga2\":" + String(vaga2Ocupada ? "true" : "false") + ",";
  json += "\"vaga3\":" + String(vaga3Ocupada ? "true" : "false") + ",";
  json += "\"livres\":" + String(vagasLivres()) + ",";
  json += "\"ocupadas\":" + String(3 - vagasLivres()) + ",";
  json += "\"cancela\":" + String(cancelaAberta ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_VAGA1, OUTPUT);
  pinMode(ECHO_VAGA1, INPUT);
  pinMode(TRIG_VAGA2, OUTPUT);
  pinMode(ECHO_VAGA2, INPUT);
  pinMode(TRIG_VAGA3, OUTPUT);
  pinMode(ECHO_VAGA3, INPUT);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_VERMELHO, HIGH);
  delay(3000);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  digitalWrite(TRIG_VAGA1, LOW);
  digitalWrite(TRIG_VAGA2, LOW);
  digitalWrite(TRIG_VAGA3, LOW);

  cancela.setPeriodHertz(50);
  cancela.attach(PINO_SERVO, 500, 2400);
  fecharCancela();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Conectando ao Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, paginaPrincipal);
  server.on("/estado", HTTP_GET, enviarEstado);

  server.on("/abrir", HTTP_GET, []() {
    atualizarVagas();

    if (vagasLivres() > 0) {
      abrirCancela();
      server.send(200, "text/plain", "Cancela aberta.");
    } else {
      server.send(409, "text/plain",
                  "Estacionamento lotado. Cancela fechada.");
    }
  });

  server.on("/fechar", HTTP_GET, []() {
    fecharCancela();
    server.send(200, "text/plain", "Cancela fechada.");
  });

  server.begin();
  Serial.println("Servidor web iniciado.");

  atualizarVagas();
}

void loop() {
  server.handleClient();

  if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = millis();
    atualizarVagas();
  }

  if (cancelaAberta &&
      millis() - instanteAbertura >= TEMPO_CANCELA_ABERTA) {
    fecharCancela();
  }
}
