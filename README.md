# DS18B20 Multi-Bus – ESP32 + W5500 Ethernet (v2)

> **v2** – Migrado de WiFi+MQTT para **Ethernet W5500 + HTTP POST**, 2 barramentos OneWire, servidor web embutido.

Leitura de até 16 sensores DS18B20 por barramento OneWire com envio de dados via **HTTP POST** e servidor web embutido para monitoramento e configuração.

---

## Hardware

### W5500 (SPI)
| Pino W5500 | GPIO ESP32 | Função             |
|-----------|-----------|---------------------|
| MOSI      | 23        | Dados Saída         |
| MISO      | 19        | Dados Entrada       |
| SCLK      | 18        | Clock               |
| SCS (CS)  | 5         | Seleção do Chip     |
| RST       | 33        | Reset               |
| INT       | 4         | Interrupção (opc.)  |

### Barramentos OneWire
| Barramento | GPIO |
|-----------|------|
| Bus 1     | 25   |
| Bus 2     | 26   |

### LEDs
| LED              | GPIO | Função                      |
|-----------------|------|-----------------------------|
| Status Rede     | 12   | Ligado = link OK            |
| Atividade Bus 1 | 14   | Pisca durante leitura Bus 1 |
| Atividade Bus 2 | 27   | Pisca durante leitura Bus 2 |

---

## Arquivos do Projeto

| Arquivo                  | Descrição                                         |
|--------------------------|---------------------------------------------------|
| `ds18b20_ESP32_eth.ino`  | Arquivo principal – setup / loop                  |
| `config.h`               | Estrutura de configuração + helpers EEPROM        |
| `ethernet.ino`           | Inicialização W5500, DHCP, status JSON            |
| `sensors.ino`            | Scan OneWire, leituras, diagnóstico de barramento |
| `http_client.ino`        | POST HTTP para servidor externo                   |
| `web_server.ino`         | Servidor HTTP embutido (UI + API REST)            |
| `leds.ino`               | Controle dos LEDs de feedback                     |

---

## Bibliotecas necessárias (Arduino IDE / PlatformIO)

- **Ethernet** (`arduino-libraries/Ethernet` ≥ 2.0 – suporte W5500)
- **OneWire** (`paulstoffregen/OneWire`)
- **DallasTemperature** (`milesburton/DallasTemperature`)
- **ArduinoJson** (`bblanchon/ArduinoJson` ≥ 6)
- **EEPROM** (inclusa no core ESP32)

---

## API REST (porta 80)

| Método | Rota          | Descrição                                         |
|--------|---------------|---------------------------------------------------|
| GET    | `/`           | Página web de monitoramento e configuração        |
| GET    | `/api/status` | JSON com leituras, status de rede e configuração  |
| GET    | `/api/diag`   | JSON com diagnóstico de qualidade dos barramentos |
| POST   | `/api/config` | Atualiza configurações (JSON body)                |
| POST   | `/api/scan`   | Re-escaneia sensores e envia lista ao servidor    |

### Exemplo: salvar configuração
```json
POST /api/config
{
  "serverHost": "192.168.1.50",
  "serverPort": 3000,
  "serverPath": "/api/sensors",
  "pollInterval": 15000,
  "deviceName": "sala_fria_01"
}
```

---

## Diagnóstico de qualidade do barramento

A rota `/api/diag` e a página web exibem a porcentagem de sensores respondentes por barramento com barra de qualidade colorida.
Isso auxilia na calibração do trimpot de pull-up conforme o comprimento do cabo.

| Qualidade | Ação sugerida                    |
|-----------|----------------------------------|
| 100 %     | Ótimo                            |
| ≥ 75 %    | Bom                              |
| ≥ 50 %    | Regular – ajuste o pull-up       |
| < 50 %    | Ruim – verifique cabos e pull-up |

---

## Configurações persistentes (EEPROM)

Salvas na EEPROM interna do ESP32 e sobrevivem a resets.
Na primeira inicialização (EEPROM vazia) os valores padrão são gravados automaticamente.

---

## Histórico
- **v1** – WiFi + MQTT (4 barramentos)
- **v2** – Ethernet W5500 + HTTP POST (2 barramentos, servidor web embutido)

---

## Legado (v1)

Código original em `ds18b20_ESP32_wif.ino` e `reconect.ino` (mantidos para referência).

---

## 🚀 Características Principais

✅ **Quatro barramentos One Wire**  
📌 Cada barramento suporta até **4 sensores DS18B20**.  
📌 Distribuídos ao longo do cabeamento para medições em múltiplos pontos.  

✅ **Cabo Blindado de 3 vias 1mm²**  
📌 Proporciona isolamento para comunicação estável em ambientes ruidosos.  

✅ **Reforço de Sinal**  
📌 Fonte de alimentação **3.3V 2A** integrada ao cabo para garantir a entrega do sinal por toda a extensão.  

✅ **Potenciômetro Multivoltas 5k Ajustável em Cada Barramento**  
📌 Regula o **pull-up** do sinal para otimizar a estabilidade da comunicação.  

✅ **Integração MQTT**  
📌 Publica os dados dos sensores em tópicos específicos para visualização e análise na web.  

---

## ⚙️ Funcionamento

1️⃣ **Inicialização**  
🔹 O ESP32 conecta-se a uma rede Wi-Fi e ao servidor MQTT.  
🔹 Detecta e lista os sensores **DS18B20** conectados em cada barramento.  

2️⃣ **Leitura de Temperatura**  
🔹 O ESP32 solicita leituras de todos os sensores e converte os dados para formato JSON.  

3️⃣ **Envio de Dados**  
🔹 Os valores das temperaturas são publicados no tópico MQTT `sensors_ds18b20/barramento_X`.  

4️⃣ **Ajuste Dinâmico**  
🔹 Os potenciômetros permitem ajustes no **pull-up** do sinal para garantir comunicação estável.  

5️⃣ **Visualização na Web**  
🔹 Um servidor MQTT coleta os dados e os plota em **gráficos para monitoramento em tempo real**.  

---

## 📡 Configurações Necessárias

### 🔗 Rede Wi-Fi
```plaintext
SSID: nome_da_rede
Senha: *********
```

### 🌐 Servidor MQTT
```plaintext
IP: 192.168.1.100
Porta: 1883
```

---

## 🛠 Código-Fonte

O código-fonte deste projeto está implementado em **C++** e pode ser encontrado neste repositório. 
Ele utiliza as bibliotecas:

📌 `WiFi` → Conexão Wi-Fi  
📌 `PubSubClient` → Integração MQTT  
📌 `ArduinoJson` → Manipulação de dados JSON  
📌 `OneWire` e `DallasTemperature` → Comunicação com sensores **DS18B20**  

---

## 🔧 Dependências

Certifique-se de instalar as seguintes bibliotecas na **IDE Arduino**:

```plaintext
1. WiFi
2. PubSubClient
3. ArduinoJson
4. OneWire
5. DallasTemperature
```

---

## 📊 Esquema Elétrico

📌 Cada **barramento One Wire** conecta até **4 sensores DS18B20**.  
📌 A alimentação e os **potenciômetros** estão localizados próximos ao **ESP32**.  

---

## 🔍 Exemplo de Resultado (JSON Publicado)

```json
{
  "barramento_1": {
    "28FF6A4C731603D3": 24.5,
    "28FF6A4C73160456": 25.0
  },
  "barramento_2": {
    "28FF6A4C73160589": 24.8
  }
}
```

---

## 🤝 Contribuições

🚀 Contribuições são bem-vindas!  
Envie um **pull request** com suas melhorias ou ideias para discussão.  

---

## 📜 Licença

📝 Este projeto é licenciado sob a **Licença MIT**.  
Consulte o arquivo LICENSE para mais detalhes.  

---

🔹 _Desenvolvido para sistemas de monitoramento de temperatura com ESP32 e sensores DS18B20._  

