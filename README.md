# 📡 Sistema de Monitoramento com ESP32 e Sensores DS18B20

## 📌 Descrição do Projeto

Este projeto utiliza um **ESP32** conectado a **quatro barramentos One Wire**, cada um suportando até **4 sensores de temperatura DS18B20** distribuídos ao longo do cabeamento. O sistema realiza medições precisas e comunica os dados via **MQTT** para um servidor, que plota os valores em um gráfico na web. 

Cada barramento possui um **potenciômetro** para ajuste do "pull-up" do sinal, garantindo estabilidade na comunicação, independentemente da distância e do número de sensores conectados.

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

