# teste_mqtt

Simulador que envia dados aleatórios de temperatura, umidade e insolação para:
- ThingSpeak (via REST)
- Broker MQTT público (HiveMQ) nos tópicos `est_01/temp`, `est_01/umid`, `est_01/solar`
- TagoIO (via MQTT) usando token do dispositivo

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
	- `est_01/chuva` (0/1 via texto "Sem Chuva"/"Chovendo")
	- `est_01/alerta` (0/1 via "off"/"on")
 Publica no TagoIO em `mqtt.tago.io:1883` no tópico `tago/data`.
 Autenticação MQTT TagoIO:
 - Host: `mqtt.tago.io`
 - Port: `1883` (use 8883 com TLS se desejar)
 - Username: `Token`
 - Password: `<Device Token>`
 - Client ID: qualquer ID único

## TagoIO
- Ajuste, se necessário, em `thingspeak_rest.py`:
  - `TAGOIO_DEVICE_TOKEN` (username do MQTT)
  - `TAGOIO_TOPIC` (padrão `tago/data`)
- Payload enviado é um array de variáveis com `variable`, `value`, `unit` (quando aplicável) e `time` em ISO8601.

Para observar as mensagens MQTT, você pode usar, por exemplo, o HiveMQ WebSocket Client:

```
"$BROWSER" https://www.hivemq.com/demos/websocket-client/
```

Conecte em `broker.hivemq.com` porta `8000` (WebSocket demo) e assine os tópicos acima.