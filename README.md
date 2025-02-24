 Sistema de Monitoramento Ambiental utilizando o **ESP32** e o protocolo **MQTT** no ESP-IDF,
 desenvolvido como um dos desafios  do curso Academia ESP32 Profissional da empresa Embarcados.
   
   Materiais e softwares utilizados
   
    ESP32 WROOM-32
    broker  EMQX Platform 
    Protoboard 
    DHT22
    MQTTX
    VSCode


 Funcionamento do Sistema:

 São utilizados o protoboard com ESP32 e o MQTTX simulando um outro protoboard.
 O protoboard com ESP32  coleta os dados de **temperatura** e **umidade** do sensor DHT22 e envia a cada 60 segundos para o broker EMQX.
 Quando a temperatura  ultrapassar um valor configurado, por exemplo, 30 graus Celsius ou a umidade abaixar de 50 porcento de umidade, uma mensagem deve ser publicada em um tópico de alarme e os valores de temperatura e umidade  são enviados em outro topico.
 o MQTTX deve subscrever-se a esses tópicos e receber essas mensagens. Para a ativação do LED, que é simulado no protoboard com o ESP32, o MQTTX publica um tópico para enviar mensagem LED_ON e LED_OFF. O MQTTX recebendo uma condição de alarme,  envia LED_ON para o ESP32  indicando que recebeu o alarme, 
 

