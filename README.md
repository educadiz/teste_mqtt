# teste_mqtt

Simulador que envia dados aleatórios de temperatura, umidade e insolação para:
- ThingSpeak (via REST)
- Broker MQTT público (HiveMQ) nos tópicos `est_01/temp`, `est_01/umid`, `est_01/solar`

## Requisitos

- Python 3.12+

Instale as dependências:

```
pip install -r requirements.txt
```

## Executar

```
python thingspeak_rest.py
```

O script:
- Publica no ThingSpeak a cada 15s (limite da API)
- Publica em MQTT no broker `broker.hivemq.com:1883` nos tópicos:
	- `est_01/temp` (temperatura em °C)
	- `est_01/umid` (umidade em %)
	- `est_01/solar` (insolação em W/m²)

Para observar as mensagens MQTT, você pode usar, por exemplo, o HiveMQ WebSocket Client:

```
"$BROWSER" https://www.hivemq.com/demos/websocket-client/
```

Conecte em `broker.hivemq.com` porta `8000` (WebSocket demo) e assine os tópicos acima.