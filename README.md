# Teste MQTT - Simulador de Estação Meteorológica

Simulador que gera dados aleatórios de uma estação meteorológica e envia para múltiplas plataformas IoT:
- **ThingSpeak** (via REST API)
- **Blynk** (via REST API)
- **MQTT Broker público** (HiveMQ)

## 🌡️ Dados Simulados

O simulador gera os seguintes dados aleatórios:
- **Temperatura**: 15.0°C a 35.0°C
- **Umidade**: 30.0% a 90.0%
- **Insolação**: 0.0% a 100.0%
- **Chuva**: Estado ("Chuvendo" / "Sem Chuva")
- **Alerta**: Estado ("on" / "off")

## 📋 Requisitos

- Python 3.12+

Instale as dependências:

```bash
pip install -r requirements.txt
```

## ⚙️ Configuração

### ThingSpeak
Ajuste no arquivo `simulador.py`:
- `WRITE_API_KEY`: Sua chave de escrita do canal ThingSpeak

### Blynk
Configure no arquivo `simulador.py`:
- `BLYNK_TEMPLATE_ID`: ID do seu template Blynk
- `BLYNK_TEMPLATE_NAME`: Nome do template
- `BLYNK_AUTH_TOKEN`: Token de autenticação do device Blynk

**Virtual Pins utilizados:**
- V0: Temperatura (°C)
- V1: Umidade (%)
- V2: Luminosidade (%)
- V3: Chuva (0/1)
- V4: Alerta (0/1)

### MQTT
O simulador usa o broker público HiveMQ com os tópicos:
- `est_01/temp`: Temperatura
- `est_01/umid`: Umidade
- `est_01/solar`: Insolação
- `est_01/chuva`: Estado da chuva
- `est_01/alerta`: Estado do alerta

## 🚀 Executar

```bash
python simulador.py
```

O script executa as seguintes ações a cada 15 segundos:
1. Gera dados aleatórios dos sensores
2. Envia para ThingSpeak (campos 1, 2 e 3)
3. Envia para Blynk (Virtual Pins V0-V4)
4. Publica nos tópicos MQTT

## 📊 Monitoramento

### ThingSpeak
Acesse seu canal no ThingSpeak para visualizar os gráficos dos dados.

### Blynk
Abra o app Blynk e visualize os widgets configurados para os Virtual Pins.

### MQTT
Para monitorar as mensagens MQTT, você pode usar:

**HiveMQ WebSocket Client:**
```bash
"$BROWSER" https://www.hivemq.com/demos/websocket-client/
```
- Conecte em `broker.hivemq.com` porta `8000`
- Assine os tópicos: `est_01/+` para todos os dados

**Mosquitto Client (se instalado):**
```bash
# Escutar todos os tópicos
mosquitto_sub -h broker.hivemq.com -t "est_01/#"

# Escutar tópico específico
mosquitto_sub -h broker.hivemq.com -t "est_01/temp"
```

## 📱 Estrutura do Projeto

```
/
├── simulador.py          # Script principal
├── main.ino             # Código Arduino (opcional)
├── main_display_ST7789.ino  # Código Arduino com display
├── requirements.txt      # Dependências Python
└── README.md            # Este arquivo
```

## 🔧 Funcionalidades

- ✅ Geração de dados aleatórios realísticos
- ✅ Envio para múltiplas plataformas IoT
- ✅ Tratamento de erros e reconexão automática
- ✅ Logs detalhados de status
- ✅ Teste de conectividade automático
- ✅ Interface de terminal amigável