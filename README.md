 Sistema de Monitoramento Ambiental utilizando o **ESP32** e o protocolo **MQTT** no ESP-IDF.
 
   
   Materiais e softwares utilizados
   
    ESP32 Node MCU
    broker o EMQX Platform 
    Protoboard 
    DHT22
    MQTTX
    VSCode


 Funcionamento do Sistema:

 O ESP32  coleta os dados de **temperatura** e **umidade** com o sensor DHT22 e envia a cada 60 segundos para o broker EMQX.
 Quando a temperatura ou a umidade ultrapassarem valores configurados, por exemplo, 30 graus celsius e 50 porcento de umidade, uma mensagem deve ser publicada em um tópico de alarme e os valores de temperatura e umidade também são enviados em outro topico.
 o MQTTX deve subscrever-se a esse tópico para receber os valores de temperatura e umidade e simular a ativação do LED para indicar o alarme.
 

