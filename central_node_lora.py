import time
import sys
import subprocess
from gpiozero import AngularServo
from time import sleep

# --- CONFIGURAÇÃO ---
SERVO_PIN = 17          # Pino GPIO do Servo
LORA_FREQ = 868.0       # Frequência (868MHz)
LORA_SF = 12            # Spreading Factor (Longo Alcance)
LORA_BW = 125000        # Bandwidth

# --- DEPENDÊNCIA LORA ---
# Este script assume a existência de uma biblioteca SX127x.
# Se estiveres a usar o Waveshare LoRa HAT, descarrega os ficheiros de exemplo
# e coloca 'SX127x.py' e 'config.py' na mesma pasta.
try:
    from SX127x import SX127x
except ImportError:
    print("ERRO: Biblioteca SX127x não encontrada.")
    print("Por favor copia os ficheiros 'SX127x.py' da demo da Waveshare para esta pasta.")
    print("Alternativa: instalar pyLoRa e adaptar o import.")
    sys.exit(1)

# --- SETUP HARDWARE ---
# Ajustar range do servo conforme o teu modelo (min_pulse_width/max_pulse_width)
servo = AngularServo(SERVO_PIN, min_angle=0, max_angle=180)

def take_photo(angle):
    filename = f"fire_alert_{int(time.time())}_ang{angle}.jpg"
    print(f"📸 Tirando foto: {filename}")
    
    # Usando libcamera-still (Bullseye/Bookworm)
    # -t 1 : timeout curto
    # -o : output file
    # --nopreview : sem preview no ecra
    cmd = ["libcamera-still", "-t", "1", "-o", filename, "--nopreview"]
    
    try:
        subprocess.run(cmd, check=True)
        print("✅ Foto guardada com sucesso.")
    except Exception as e:
        print(f"❌ Erro ao tirar foto: {e}")

class LoRaReceiver(SX127x):
    def __init__(self, verbose=False):
        super(LoRaReceiver, self).__init__(board_config=BOARD, verbose=verbose)
        self.set_mode(MODE.SLEEP)
        self.set_dio_mapping([0] * 6)
        self.set_pa_config(pa_select=1)
        self.set_freq(LORA_FREQ)
        self.set_spreading_factor(LORA_SF)
        self.set_bw(LORA_BW)
        self.set_sync_word(0xF3) # MESMO SYNC WORD DO ESP32!
        self.set_rx_crc(True)
        
    def on_rx_done(self):
        print("\n📡 Pacote Recebido!")
        self.clear_irq_flags(RxDone=1)
        payload = self.read_payload(nocheck=True)
        
        # Converter bytes para string
        try:
            message = bytes(payload).decode("utf-8", 'ignore')
            print(f"📩 Conteúdo: {message}")
            
            # Parsing "ID:1,ANG:45"
            if "ID:" in message and "ANG:" in message:
                parts = message.split(",")
                angle_part = [p for p in parts if "ANG:" in p][0]
                angle_val = int(angle_part.split(":")[1])
                
                print(f"🔥 ALERTA DE FOGO! Sensor no ângulo {angle_val}°")
                self.process_alert(angle_val)
            else:
                print("⚠️ Formato inválido ou ruído.")
                
        except Exception as e:
            print(f"❌ Erro no parsing: {e}")
            
        self.set_mode(MODE.SLEEP)
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT)

    def process_alert(self, angle):
        # 1. Mover Servo
        print(f"⚙️ Movendo servo para {angle}°...")
        servo.angle = angle
        sleep(1) # Esperar o servo chegar
        
        # 2. Tirar Foto
        take_photo(angle)
        
        # 3. Resetar servo
         servo.angle = 90

# --- CONFIGURAÇÃO DA PLACA (Waveshare HAT) ---
# Isto depende da biblioteca config.py do Waveshare, mas aqui definimos um default
class BoardConfig:
    DIO0 = 4   # BC4
    RST  = 17  # BC17  <- ATENÇÃO: Se o servo usar o 17, muda isto!
    LED  = 18  # Led debug
    # SPI
    SPI_BUS  = 0
    SPI_CS   = 0

# NOTA IMPORTANTE: GPIO 17 é muitas vezes o Reset do LoRa HAT e também default para PWM.
# Se houver conflito, muda o pino do Servo para GPIO 27 ou outro livre.
# Vou mudar o Servo para GPIO 22 para evitar conflitos com o HAT default.
SERVO_PIN = 22 
servo = AngularServo(SERVO_PIN, min_angle=0, max_angle=180)

BOARD = BoardConfig()

def start():
    print("🌲 Iniciando Central Node (LoRa Receiver)...")
    print(f"ℹ️ Freq: {LORA_FREQ}MHz, SF: {LORA_SF}")
    
    lora = LoRaReceiver(verbose=False)
    lora.set_mode(MODE.STDBY)
    lora.set_mode(MODE.RXCONT)
    
    print("👂 Escutando...")
    try:
        while True:
            sleep(0.1)
            # O processamento é feito no callback on_rx_done (interrupção/polling interno)
            # Se a lib não usar thread, precisamos de chamar o handler
            # lora.checkForRx() # Descomentar se a lib não for threaded
            pass
    except KeyboardInterrupt:
        print("A sair...")
        lora.set_mode(MODE.SLEEP)

if __name__ == "__main__":
    start()
