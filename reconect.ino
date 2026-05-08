
int reboot = 0;
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. MQTT aguardando reconexao da rede.");
      return;
    }

   Serial.println(" MQTT connection... ");
  
    if (client.connect("sensors_ds18b20_ESP32")) {
     Serial.println("connected MQTT");
      reboot = 0;
      // Once connected, publish an announcement...
     client.publish("sensors_ds18b20_esp32", "sensors_ds18b20_esp32 Online");

 //------- subscribes --------------//
client.subscribe("reset_ds18b20_esp32");
client.subscribe("timeToSend_ds18b20_esp32");
 //------- subscribes --------------//

delay(100);

    } else {
     Serial.print("FALHA, client status = ");
     Serial.print(client.state());
     Serial.print(" / TENTATIVA, Nº");
     Serial.print(reboot);
     Serial.println(" ...");
     reboot++;                         
    Serial.println("tentando se conectar ao server..");  
     digitalWrite(LED_5, HIGH);
     delay(1000); 
     digitalWrite(LED_5, LOW);
     delay(1000); 
      if(reboot >= 10){ ESP.restart();}
    }
  }
}
