import os, sys, cv2, time, socket, threading
from LoRaRF import SX126x
from gpiozero import Servo
from gpiozero.pins.lgpio import LGPIOFactory

# --- CONFIGURAÇÕES ---
UDP_PORT = 5005           # Porta para receber WiFi do S2
COOLDOWN_TIME = 120       # 10 minutos em segundos
POS_ESQUERDA = 0.8        # Posição para ID 1 (S2)
POS_DIREITA = -0.8        # Posição para ID 2 (S3)
POS_CENTRO = 0.0          # Vigilância

# --- ESTILIZAÇÃO ---
C_GREEN, C_YELLOW, C_RED = "\033[92m", "\033[93m", "\033[91m"
C_BOLD, C_RESET = "\033[1m", "\033[0m"

# --- HARDWARE ---
factory = LGPIOFactory()
servo = Servo(12, pin_factory=factory, min_pulse_width=0.5/1000, max_pulse_width=2.5/1000)
lock = threading.Lock() # Impede conflito entre LoRa e WiFi no servo/camera
alert_history = {}      # Guarda {id: timestamp}

def processar_alerta(device_id):
    """Executa a rotação, foto e cooldown"""
    agora = time.time()

    # Validação do Cooldown
    if device_id in alert_history:
        if agora - alert_history[device_id] < COOLDOWN_TIME:
            print(f"{C_YELLOW}[Cooldown] Alerta de ID {device_id} ignorado (menos de 10min).{C_RESET}")
            return

    with lock: # Bloqueia o hardware para este processo
        alert_history[device_id] = agora
        print(f"{C_RED}{C_BOLD}!!! ALERTA ATIVADO PARA ID {device_id} !!!{C_RESET}")

        # 1. Rodar para o alvo
        pos = POS_ESQUERDA if device_id == 1 else POS_DIREITA
        servo.value = pos
        time.sleep(1.5)
        servo.detach() # Para o tremor

        # 2. Captura de Foto
        cap = cv2.VideoCapture(0)
        ret, frame = cap.read()
        if ret:
            nome_foto = f"/home/mmenezes/FOGO_ID{device_id}_{time.strftime('%H%M%S')}.jpg"
            cv2.imwrite(nome_foto, frame)
            print(f"{C_GREEN}Foto guardada: {nome_foto}{C_RESET}")
        cap.release()

        # 3. Voltar ao Centro
        time.sleep(2)
        print("Retornando à vigilância...")
        servo.value = POS_CENTRO
        time.sleep(1.5)
        servo.detach()

def wifi_server():
    """Thread dedicada para ouvir alertas via WiFi"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    print(f"{C_GREEN}[WiFi] Servidor ativo na porta {UDP_PORT}{C_RESET}")

    while True:
        data, addr = sock.recvfrom(1024)
        msg = data.decode().strip()

        # --- ADICIONA ESTA LINHA PARA VERES TUDO NO TERMINAL ---
        print(f"[{time.strftime('%H:%M:%S')}] WiFi: {msg}")

        if "FIRE-DETECTED" in msg:
            try:
                did = int(msg.split("ID:")[1].split(",")[0])
                processar_alerta(did)
            except: pass

def lora_loop():
    """Loop principal para ouvir LoRa com limpeza de buffer"""
    LoRa = SX126x()
    if not LoRa.begin(0, 0, 18, 20, 16, 6, -1):
        raise Exception("Erro ao iniciar LoRa")

    LoRa.setDio2RfSwitch()
    LoRa.setFrequency(868000000)
    LoRa.setLoRaModulation(7, 125000, 5)
    LoRa.setLoRaPacket(LoRa.HEADER_EXPLICIT, 8, 64, True)
    LoRa.setSyncWord(0xF344)
    LoRa.request(LoRa.RX_CONTINUOUS)

    # --- LIMPEZA ROBUSTA DO BUFFER ---
    print(f"{C_YELLOW}A limpar buffer LoRa (ignorando mensagens antigas)...{C_RESET}")
    start_clear = time.time()
    while time.time() - start_clear < 3: # Limpa durante 3 segundos
        if LoRa.available():
            while LoRa.available() > 0: LoRa.read()

    print(f"{C_GREEN}[LoRa] Receptor ativo e limpo em 868MHz{C_RESET}")

    while True:
        if LoRa.available():
            message = ""
            while LoRa.available() > 0:
                val = LoRa.read()
                if 32 <= val <= 126: message += chr(val)

            # --- MUDANÇA CRÍTICA: Imprimir SEMPRE o que chega ---
            timestamp = time.strftime('%H:%M:%S')
            print(f"[{timestamp}] LoRa RAW: {message}")

            if "FIRE-DETECTED" in message:
                try:
                    # Extrai o ID e processa
                    did = int(message.split("ID:")[1].split(",")[0])
                    processar_alerta(did)
                except Exception as e:
                    print(f"Erro ao processar ID: {e}")

# --- EXECUÇÃO ---
servo.value = POS_CENTRO # Inicia centrado
time.sleep(1)
servo.detach()

# Inicia WiFi em segundo plano
t_wifi = threading.Thread(target=wifi_server, daemon=True)
t_wifi.start()

# Inicia LoRa no loop principal
try:
    lora_loop()
except KeyboardInterrupt:
    print("\nSistema encerrado.")
finally:
    servo.detach()
