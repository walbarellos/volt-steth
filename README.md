# GroundHound 🐶⚡ (ou HEAD: Hardware Electrical Anomaly Detector)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)](https://kernel.org)
[![Build & Tests](https://img.shields.io/badge/Tests-Passing%20100%25-brightgreen.svg)]()

> **O "estetoscópio elétrico" do seu computador.**  
> Detecte loops de terra (ground loops), ruído de interferência eletromagnética (EMI/coil whine), oscilações de voltagem (VRM ripple) e integridade física de barramento diretamente pelo Linux.

---

## 📖 Por que este projeto existe?

Quase todo mundo já passou por isso: você pluga uma caixinha de som ou headset barato no PC que usa **cabo P2 (áudio) + USB (energia)** e o som fica repleto de **estática, zumbido e chiados ensurdecedores** toda vez que o mouse mexe ou a placa de vídeo trabalha. 

Isso acontece porque a caixinha fecha um **Ground Loop (circuito fechado de terra)** com a placa-mãe. A corrente parasita e os harmônicos de rede da fonte vazam diretamente para a malha analógica de áudio.

O **GroundHound** foi concebido para transformar a entrada analógica e os sensores elétricos do computador em uma ferramenta de diagnóstico físico em tempo real.

---

## 🛠️ O que ele detecta?

1. **Loop de Terra (Mains Hum):**
   * Processa o sinal via **algoritmo de Goertzel com janela de Hann**, caçando as frequências fundamentais e harmônicas da rede elétrica local ($60\text{ Hz}$, $120\text{ Hz}$, $180\text{ Hz}$ no Brasil/EUA ou $50\text{ Hz}$ na Europa).
   * Alerta imediatamente se o sinal de 60Hz/120Hz sobressair com significância matemática acima do piso de ruído do ambiente.
2. **Ripple e Instabilidade de VRM / Fonte:**
   * Amostra os sensores de voltagem do subsistema de kernel `/sys/class/hwmon` e calcula o desvio padrão ($\sigma$) e a tensão pico-a-pico ($V_{\text{pp}}$) em janelas móveis temporais.
3. **Acoplamento Eletromagnético (EMI & Coil Whine):**
   * Calcula o **coeficiente de correlação de Pearson** entre picos de ruído de alta frequência (2 kHz a 12 kHz) e pulsos de potência instantânea da CPU/GPU (via Intel RAPL / microjoules).
4. **Erros de Barramento PCIe (AER):**
   * Monitora em tempo real os contadores do protocolo PCIe AER (Advanced Error Reporting) para falhas físicas na trilha do slot ou mau contato.

---

## 🏗️ Arquitetura de Software (Pipe-and-Filter)

O projeto segue os princípios **SOLID**, **Orientação a Objetos estrita** e o padrão arquitetural **Pipe-and-Filter**:

```
[ Sources ]                       [ Filters ]                          [ Sinks ]
┌──────────────────┐           ┌────────────────────────────┐       ┌─────────────────┐
│ AlsaAudioSource  │──────┐    │ GroundLoopDetector         │   ┌──>│  ConsoleHudSink │
└──────────────────┘      │    └────────────────────────────┘   │   └─────────────────┘
┌──────────────────┐      ├───>┌────────────────────────────┐───┤
│ HwmonPowerSource │──────┼───>│ VoltageRippleDetector      │───┤   ┌─────────────────┐
└──────────────────┘      │    └────────────────────────────┘   └──>│  JsonStreamSink │
┌──────────────────┐      │    ┌────────────────────────────┐       └─────────────────┘
│  PcieAerSource   │──────┘    │ EmiCorrelationDetector     │
└──────────────────┘           └────────────────────────────┘
```

* **Sem Hardcode:** Todos os limiares, taxas, frequências e diretórios do kernel são lidos dinamicamente de um arquivo `.ini` configurável.
* **Tolerância a Falhas:** Se o microfone ou sensor de hardware não estiver disponível, o pipeline continua executando sem abortar o processo (degradação graciosa).

---

## 🚀 Compilação e Execução

### Pré-requisitos (Arch / Omarchy Linux):
```bash
sudo pacman -S base-devel cmake alsa-lib
```

### Compilar:
```bash
cmake -B build
cmake --build build
```

### Rodar os Testes Automatizados:
```bash
cd build && ctest --output-on-failure
# ou diretamente:
./build/test-pipeline
```

### Executar a HUD no Terminal:
```bash
./build/head-detector --config config.default.ini
```

### Exportar em Streaming JSON (para scripts e automação):
```bash
./build/head-detector --config config.default.ini --json
```

---

## ⚙️ Arquivo de Configuração (`config.default.ini`)

```ini
[audio]
device = default
sample_rate = 48000
buffer_size = 4096
mains_frequency_hz = 60.0
hum_ratio_threshold = 3.0

[power]
hwmon_base_dir = /sys/class/hwmon
max_ripple_stddev_mv = 35.0
sample_history_size = 50

[bus]
pci_base_dir = /sys/bus/pci/devices
check_aer_errors = true

[output]
enable_console_hud = true
enable_json_stream = false
hud_refresh_rate_ms = 150
```

---

## 📄 Licença
Distribuído sob a licença MIT.
