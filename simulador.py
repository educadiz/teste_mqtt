import requests
import random
import time
from datetime import datetime
import paho.mqtt.client as mqtt
import json

# CONFIGURACAO THINGSPEAK:
WRITE_API_KEY = "2PI8MD4NVFEY9XSZ"                  # Chave de escrita do canal
BASE_URL = "https://api.thingspeak.com/update.json" # Endpoint REST ThingSpeak

# CONFIGURACAO BLYNK:
BLYNK_TEMPLATE_ID = "TMPL2EzjqzR7P"                     # ID do template Blynk
BLYNK_TEMPLATE_NAME = "EMC01"                           # Nome do template Blynk
BLYNK_AUTH_TOKEN = "y0YIz5He8zrIW1BF-0eHwQAN7VdxJkAK"   # Token de autenticação Blynk
BLYNK_BASE_URL = "https://blynk.cloud/external/api"     # URL oficial de produção

# VIRTUAL PINS BLYNK:
# Abaixo são apenas exemplos de Virtual Pins usados no projeto:
BLYNK_PINS = {
    "temperatura": "V0",    # V0 = Temperatura
    "umidade": "V1",        # V1 = Umidade  
    "luminosidade": "V2",   # V2 = Luminosidade
    "chuva": "V3",          # V3 = Chuva (0/1)
    "alerta": "V4"          # V4 = Alerta (0/1)
}

# CONFIGURACAO MQTT EXTERNO: BROKER / PORTA TCP / TOPICOS:
# Abaixo são apenas tópicos exemplo para HiveMQ public broker:
MQTT_BROKER_HOST = "broker.hivemq.com"  
MQTT_BROKER_PORT = 1883
MQTT_TOPICS = {
    "temp": "est_01/temp",
    "umid": "est_01/umid",
    "solar": "est_01/solar",
    "chuva": "est_01/chuva",
    "alerta": "est_01/alerta",
}

def build_mqtt_client() -> mqtt.Client:
    
    client = mqtt.Client(client_id=f"simulator-{random.randint(1000, 9999)}", clean_session=True)
    
    def on_connect(cl, userdata, flags, rc, properties=None):
        status = "success" if rc == 0 else f"failed rc={rc}"
        print(f"[MQTT] Connect {status} to {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")

    def on_disconnect(cl, userdata, rc, properties=None):
        if rc != 0:
            print("[MQTT] Unexpected disconnect, will auto-reconnect…")
        else:
            print("[MQTT] Disconnected")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect

    client.loop_start()

    # TENTATIVA DE CONEXAO INICIAL:
    try:
        client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, keepalive=30)
    except Exception as e:
        print(f"[MQTT] Initial connect error: {e}")

    return client

def generate_random_data():
    # GERADOR DE DADOS RANDOMICOS
    temperature = round(random.uniform(15.0, 35.0), 2)      # TEMPERATURA
    humidity = round(random.uniform(30.0, 90.0), 2)         # HUMIDADE
    insolation = round(random.uniform(0.0, 100.0), 2)       # SOLAR
    return temperature, humidity, insolation

def generate_random_rain_state() -> str:
    
    return random.choice(["Chuvendo", "Sem Chuva"])

def generate_random_temp_alert() -> str:
    
    return random.choice(["on", "off"])

def send_to_blynk(temp, humidity, insolation, rain_state, alert_state):
    """Envia dados para Blynk usando a API externa oficial"""
    try:
        # Converte estados para valores numéricos para Blynk:
        chuva_numeric = 1 if rain_state == "Chuvendo" else 0
        alerta_numeric = 1 if alert_state == "on" else 0
        
        # Dados para enviar para cada Virtual Pin
        blynk_data = {
            BLYNK_PINS["temperatura"]: temp,
            BLYNK_PINS["umidade"]: humidity,
            BLYNK_PINS["luminosidade"]: insolation,
            BLYNK_PINS["chuva"]: chuva_numeric,
            BLYNK_PINS["alerta"]: alerta_numeric
        }
        
        success_count = 0
        total_pins = len(blynk_data)
        
        # Envia cada valor para seu Virtual Pin correspondente
        for pin, value in blynk_data.items():
            # URL correta da API do Blynk
            url = f"{BLYNK_BASE_URL}/update"
            
            # Parâmetros corretos para a API
            params = {
                'token': BLYNK_AUTH_TOKEN,
                pin: value
            }
            
            # Headers recomendados
            headers = {
                'User-Agent': 'EstacaoMeteorologica/1.0',
                'Accept': 'application/json'
            }
            
            response = requests.get(url, params=params, headers=headers, timeout=15)
            
            if response.status_code == 200:
                success_count += 1
                print(f"[BLYNK] ✅ {pin}: {value} - Enviado")
            else:
                print(f"[BLYNK] ❌ {pin}: {value} - Erro {response.status_code}")
                if response.text:
                    print(f"[BLYNK] Response: {response.text[:100]}")
        
        # Status final do envio Blynk
        if success_count == total_pins:
            print(f"[BLYNK] ✅ Todos os Virtual Pins atualizados ({success_count}/{total_pins})")
        else:
            print(f"[BLYNK] ⚠️  Envio parcial ({success_count}/{total_pins})")
            
        return success_count == total_pins
        
    except requests.exceptions.Timeout:
        print("[BLYNK] ❌ Timeout - Servidor Blynk não respondeu (15s)")
        return False
    except requests.exceptions.ConnectionError:
        print("[BLYNK] ❌ Erro de conexão - Verifique internet")
        return False
    except requests.exceptions.RequestException as e:
        print(f"[BLYNK] ❌ Erro de requisição: {e}")
        return False
    except Exception as e:
        print(f"[BLYNK] ❌ Erro inesperado: {e}")
        return False

def test_blynk_connection():
    """Testa a conectividade com o servidor Blynk"""
    try:
        print("🔍 Testando conexão com Blynk...")
        
        # Testa endpoint básico
        test_url = f"{BLYNK_BASE_URL}/get"
        params = {
            'token': BLYNK_AUTH_TOKEN,
            'V0': ''  # Tenta ler V0
        }
        
        response = requests.get(test_url, params=params, timeout=10)
        
        if response.status_code == 200:
            print("✅ Blynk: Conexão OK")
            return True
        elif response.status_code == 400:
            print("⚠️  Blynk: Token válido, mas erro nos parâmetros")
            return False
        elif response.status_code == 401:
            print("❌ Blynk: Token inválido ou expirado")
            return False
        else:
            print(f"❌ Blynk: Erro {response.status_code}")
            return False
            
    except Exception as e:
        print(f"❌ Blynk: Erro de teste - {e}")
        return False

def main():
    headers = {
        'Content-Type': 'application/json'
    }

    mqtt_client = build_mqtt_client()
    
    print("=== SIMULADOR ESTACAO METEOROLOGICA ===")
    print(f"📊 ThingSpeak: Canal {WRITE_API_KEY[:8]}...")
    print(f"📱 Blynk: Template {BLYNK_TEMPLATE_ID} - {BLYNK_TEMPLATE_NAME}")
    print(f"🔑 Token: {BLYNK_AUTH_TOKEN[:8]}...")
    print(f"📡 MQTT: {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")
    print("🔄 Enviando dados a cada 15 segundos...")
    print("=" * 50)
    
    # Testa conectividade Blynk antes de iniciar
    print("\n🔧 Testando conectividades...")
    blynk_available = test_blynk_connection()
    
    if not blynk_available:
        print("⚠️  Continuando sem Blynk...")
    
    print("=" * 50)

    try:
        while True:
            # CHAMA FUNCAO DOS RANDOMICOS
            temp, hum, insol = generate_random_data()
            rain_state = generate_random_rain_state()
            alert_state = generate_random_temp_alert()
            
            # PEGA A HORA VIA TIMESTAMP
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            # PREPARA OS DADOS PARA ENVIAR AO THINGSPEAK VIA JSON
            data = {
                'api_key': WRITE_API_KEY,
                'field1': str(temp),
                'field2': str(hum),
                'field3': str(insol)
            }
            
            # ENVIA DADOS PARA O THINGSPEAK
            print(f"\n⏰ {timestamp}")
            print(f"🌡️  Temperature: {temp}°C")
            print(f"💧 Humidity: {hum}%")
            print(f"☀️  Insolation: {insol}%")
            print(f"🌧️  Chuva: {rain_state}")
            print(f"🚨 Alerta: {alert_state}")
            print("-" * 30)
            
            # ENVIO PARA THINGSPEAK
            print("📊 Enviando para ThingSpeak...")
            response = requests.post(BASE_URL, json=data, headers=headers, timeout=10)
            if response.status_code == 200:
                print("✅ ThingSpeak: Dados enviados com sucesso!")
            else:
                print(f"❌ ThingSpeak: Falha no envio. Status: {response.status_code}")
                print(f"Response: {response.text}")
            
            # ENVIO PARA BLYNK
            print("📱 Enviando para Blynk...")
            if blynk_available:
                blynk_success = send_to_blynk(temp, hum, insol, rain_state, alert_state)
            else:
                print("[BLYNK] ⏭️  Pulando envio - conexão indisponível")
                blynk_success = False

            # PUBLICA TOPICOS NO MQTT EXTERNO (HiveMQ demo)
            print("📡 Enviando para MQTT...")
            try:
                mqtt_client.publish(MQTT_TOPICS["temp"], str(temp), qos=0, retain=False)
                mqtt_client.publish(MQTT_TOPICS["umid"], str(hum), qos=0, retain=False)
                mqtt_client.publish(MQTT_TOPICS["solar"], str(insol), qos=0, retain=False)
                mqtt_client.publish(MQTT_TOPICS["chuva"], rain_state, qos=0, retain=False)
                mqtt_client.publish(MQTT_TOPICS["alerta"], alert_state, qos=0, retain=False)
                print("✅ MQTT: Todos os tópicos publicados com sucesso!")
            except Exception as e:
                print(f"❌ MQTT: Erro na publicação: {e}")
            
            # RESUMO DO CICLO
            print("=" * 45)
            print("📈 Resumo do envio:")
            print(f"   ThingSpeak: {'✅ OK' if response.status_code == 200 else '❌ FALHA'}")
            print(f"   Blynk: {'✅ OK' if blynk_success else '❌ FALHA'}")
            print(f"   MQTT: ✅ OK")
            print("🔄 Próximo envio em 15 segundos...")
            print("=" * 45)

            
            # AGUARDA 15 SEGUNDOS PARA ATUALIZACAO DE POSTAGEM
            time.sleep(15)
    except KeyboardInterrupt:
        print("\n🛑 Simulação interrompida pelo usuário...")
        print("📊 Encerrando conexões...")
    except Exception as e:
        print(f"❌ Erro inesperado: {e}")
    finally:
        # DESCONECTA MQTT
        try:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
            print("✅ MQTT desconectado com sucesso")
        except Exception:
            pass
        print("👋 Simulador finalizado!")

if __name__ == "__main__":
    main()
