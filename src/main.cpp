//#include <Wire.h> //Biblioteca para I2C
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <esp_wifi.h>

#pragma region variaveis globais e chamadas e ponteiros

BLECharacteristic *characteristicTX; //através desse objeto iremos enviar dados para o client

std::string tipoLixo = ""; //seta uma variavel para conter o valor que vem do bluetooth 3C:61:05:13:A8:9A

bool deviceConnected = false; //controle de dispositivo conectado
unsigned long tempoConectouOuBipou;

uint8_t newMACAddress[] = {0x3C, 0x61, 0x05, 0x13, 0xA8, 0x99};

#define SERVICE_UUID "ab0828b1-198e-4351-b779-901fa0e0371e"           // UART service UUID
#define CHARACTERISTIC_UUID_RX "4ac8a682-9736-4e5d-932b-e9b31405049c" //UUID PARA RECEBER DADOS DO BLE
#define CHARACTERISTIC_UUID_TX "0972EF8C-7613-4075-AD52-756F33D4DA91" //UUID PARA ENVIAR DADOS

#define LED 2

BLEServerCallbacks *m_pServerCallbacks = nullptr; //seta os callbacks

int trig;
unsigned long previousMicros;
unsigned long tempoEntrada;
unsigned long tempoBipou;
//20000 = 1 ms, eh o timeout para medir no máximo 34 cm, se aprofundar a lixeira, tem que aumentar esse valor
//quanto menor, mais rápido o ultrassom é lido novamente, logo, pega mais rapido objeto que cai
unsigned long timeout = 2000UL;
int contagem = 0;
int distancia = 0;
int tempoEspera = 10;  // tempo espera em segundos até desistir de esperar o lixo
int tempoDesiste = 10; // tempo espera em segundos até desistir de esperar bipar
String estado = "DESCANSE";

#pragma endregion

#pragma region pinos

#define trigPin 26   // Pin 4 trigger output
#define echoPin 25   // Pin 3 Echo input
#define onBoardLED 2 // Pin 2 onboard LED
#define pintrig1 1
#define pintrig2 2
#define pintrig3 3
#define pintrig4 4

// Ultrasonic ultrasonic(trigPin, echoPin); // Instância chamada ultrasonic com parâmetros (trig,echo)
// int ultra;
#pragma endregion

#pragma region funcoes sensor
int distanciaObjeto(int tipo)
{
  //Decide, com base no tipo de lixo, qual pino é o trigger do ultrassom na lixeira correspondente ao tipo de lixo

  switch (tipo)
  {
  case 1:
    trig = pintrig1;
    break;
  case 2:
    trig = pintrig2;
    break;
  case 3:
    trig = pintrig3;
    break;
  case 4:
    trig = pintrig4;
    break;
  default:
    trig = trigPin;
    break;
  }

  //Solta um pulso e espera o timeout ou o retorno.
  //Quanto menor o timeout, menor a distancia máxima a ser medida
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  //espera
  previousMicros = micros();
  while (!digitalRead(echoPin) && (micros() - previousMicros) <= timeout)
    ; // wait for the echo pin HIGH or timeout
  previousMicros = micros();
  while (digitalRead(echoPin) && (micros() - previousMicros) <= timeout)
    ; // wait for the echo pin LOW or timeout

  return (micros() - previousMicros) / 58; //distancia cm
}

bool esperarCair(int tipo)
{
  tempoEntrada = micros();
  while (1)
  {
    distancia = distanciaObjeto(6);
    Serial.println(distancia);
    if (distancia < 20) //distancia do obstaculo fixo, outro lado da lixeira
    {
      Serial.println("dCAIUUUUUUUU");

      return true;
    }
    if (micros() - tempoEntrada > tempoEspera * 1000000)
    {
      return false;
    }
    delay(7);
    //tempo de espera até o reverb passar. Quanto menos tempo esperar, menor a chance de ter caido outro objeto no intervalo
    //se usar material absorvedor no lado oposto, pode diminuir este tempo.
  }
}
#pragma endregion

#pragma region aqui é como se faz a thread
void loop2(void *z) //Atribuímos o loop2 ao core 0, com prioridade 1 para observar a lixeira quando quisermos
{

  while (estado == "OBSERVE")
  {
    if (esperarCair(6)) //argumento da funcao e o tipo da lixeira em int. botei 6 pra cair no default do switch e usar o pino de teste ultrasom
    {
      Serial.println(contagem);
      //Serial.println("algo caiu na lixeira. Aqui vou setar o estado para CAIU, quando ler, bote pra DESCANSE, se quiser checar denovo, bote OBSERVE ");
      estado = "CAIU";
      //Serial.println(estado);
    }
    else
    {
      //Serial.println("Não caiu nada. Aqui vou setar a variável estado para NADA, quando ler, bote pra DESCANSE, se quiser checar denovo, bote OBSERVE ");
      estado = "NADA";
      //Serial.println(estado);
    }
  }
  // vTaskDelay(1);

  vTaskDelete(NULL);
}
#pragma endregion

#pragma region regra pra fechar todas as lixeiras abertas e enviar o total de lixo para o celular,
void fechaOutrasLixeirasAbertas()
{

  /* AQUI VC FECHA TODAS AS LIXEIRAS E ABRE A QUE ESTÁ NA VARIAVEL tipoLixo
  */
}
#pragma endregion

#pragma region verifica se tem um obejto na lixeira
std::string checaSeTemLixo(std::string lixeira)
{

  /* AQUI VERIFICA SE RECONHECEU UM OBJETO DENTRO DA LIXEIRA, Uma funcao só , mas tem que conterver o lixeira string para variaveis int de 1 a 4
  */
  xTaskCreatePinnedToCore(loop2, "loop2", 2048, NULL, 8, NULL, PRO_CPU_NUM); //Cria a tarefa "loop2()" com prioridade 1, atribuída ao core 0

  estado = "OBSERVE";
  while (estado == "OBSERVE")
  {
    delay(1); //se quiser ficar esperando cair
  }
  if (estado == "CAIU")
    return "ok";
  if (estado == "NADA")
    return "fail";
  //se nao quiser esperar cair, tira o return daqui, tira o retorno no BLE da funcao lixeira,
  //e fica checando esporadicamente enquanto faz outra coisa, quando houver mudança de estado, faz a chamada no BLE
}
#pragma endregion

#pragma region regra que identifica a lixeira e retorna pra o celular se reconhceu
void bipou(std::string tipoLixo)
{
  /*    
      1° ABRI A TAMPA DA LIXEIRA QUE ESTÁ NA VARIAVEL tipoLixo
      2° CHECA SE JOGARAM O LIXO ALI MESMO
      */

  tempoConectouOuBipou = micros();
  fechaOutrasLixeirasAbertas(); //CHAMA A FUNÇÃO PARA FECHAR AS LIXEIRAS

  //"{'material': {1},'lixo': {"+retorno+"}}"
  std::string retorno = checaSeTemLixo(tipoLixo); /*retornos: ok,fail,close*/
  characteristicTX->setValue(retorno);            //seta o valor que a caracteristica notificará (enviar)
  characteristicTX->notify();
  estado = "DESCANSE";
}
#pragma endregion

#pragma region callback para receber os eventos de conexão de dispositivos

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("Desconectado");
    digitalWrite(LED,LOW);
    deviceConnected = false;
  }
};
#pragma endregion

#pragma region aqui é a classe que fica escultando caso receba alguma coisa pelo bluetooth
//callback  para eventos das características
class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    std::string rxValue = characteristic->getValue(); //pega o valor que foi enviado do celular

    tipoLixo = ""; //reseta a variavel pra deixar vazio antes de preencher

    if (rxValue.length() > 0) //verifica se existe dados (tamanho maior que zero)
    {

      for (int i = 0; i < rxValue.length(); i++)
      {

        tipoLixo = tipoLixo + rxValue[i]; //monta o array de bytes em uma string tipoLixo
      }

      Serial.print(tipoLixo.c_str());

      if (tipoLixo == "1")
      {
        Serial.println("aqui");

        bipou(tipoLixo);
      }
    }
  }
};
#pragma endregion

#pragma region setup
void setup()
{
  pinMode(trigPin, OUTPUT);    // Trigger pin set to output
  pinMode(echoPin, INPUT);     // Echo pin set to input
  pinMode(onBoardLED, OUTPUT); // Onboard LED pin set to output
  pinMode(LED, OUTPUT);

  Serial.begin(9600);

  // WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, &newMACAddress[0]);

  // Serial.printf("\nsetup() em core: %d", xPortGetCoreID());        //Mostra no monitor em qual core o setup() foi chamado

  // Create the BLE Device
  BLEDevice::init("ESP32-BLE"); // nome do dispositivo bluetooth
  BLEDevice::setMTU(500);
  // Create the BLE Server
  BLEServer *server = BLEDevice::createServer(); //cria um BLE server

  server->setCallbacks(new ServerCallbacks()); //seta o callback do server
  // Create the BLE Service
  BLEService *service = server->createService(SERVICE_UUID);
  // Create a BLE Characteristic para envio de dados
  characteristicTX = service->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY);

  characteristicTX->addDescriptor(new BLE2902());

  // Create a BLE Characteristic para recebimento de dados
  BLECharacteristic *characteristic = service->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE);

  characteristic->setCallbacks(new CharacteristicCallbacks());
  // Start the service
  service->start();
  // Start advertising (descoberta do ESP32)
  server->getAdvertising()->start();

  Serial.println("Waiting a client connection to notify...");

  delay(1);
}
#pragma endregion

#pragma region loop
void loop()
{

  Serial.println(estado);

  if (micros() - tempoConectouOuBipou > tempoDesiste * 1000000)
  {
    characteristicTX->setValue("close"); //expirou tempo após conxão ou ultimo bipe
    characteristicTX->notify();
  }

  if (deviceConnected)
  {
    tempoConectouOuBipou = micros();
    Serial.print("CONECTADO: ");
    digitalWrite(LED,HIGH);
    Serial.println();
  }

  delay(500);
}
#pragma endregion