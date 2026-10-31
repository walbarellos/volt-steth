# VoltSteth ⚡🩺

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![GTK4 & Libadwaita](https://img.shields.io/badge/UI-GTK4%20%2F%20Libadwaita-purple.svg)](https://gnome.pages.gitlab.gnome.org/libadwaita/)
[![Omarchy Linux](https://img.shields.io/badge/Omarchy-Bar%20Plugin%20Ready-yellow.svg)](https://omarchy.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)](https://kernel.org)
[![Tests: 100% Passed](https://img.shields.io/badge/Tests-Passing%20100%25-brightgreen.svg)]()

> **O estetoscópio elétrico e físico de baixo nível para Linux.**  
> Diagnóstico em tempo real de loops de terra (*ground loops*), ruído de interferência eletromagnética (EMI/coil whine), instabilidade de VRMs e integridade física de barramento diretamente pelo kernel e subsistema analógico.

---

## 🖥️ Interface Gráfica Nativa (UI/UX)

<p align="center">
  <img src="assets/ui.jpg" alt="VoltSteth Interface Gráfica GTK4 Libadwaita" width="620" />
</p>

Para quem busca uma experiência de desktop limpa e intuitiva, o **VoltSteth** conta com uma interface gráfica nativa em **GTK4 + Libadwaita** com tema escuro e suporte nativo a Wayland:

* **VU Meter Balístico com Peak-Hold:** Decaimento suave que congela picos visuais, tornando variações elétricas perfeitamente legíveis para o olho humano.
* **Mapeamento Topológico de Hardware:** Exibe placas analógicas (Realtek ALC897) vs digitais (NVIDIA HDMI) e controladores USB 3.2 xHCI conectados.
* **Calibração de Sensor com 1 Clique:** Evita falsos negativos desmutando e ajustando o ganho de captura para 80%.
* **Auditoria Automatizada de 6 Segundos:** Coleta dados físicos contínuos e emite um laudo conclusivo com veredito direto.

---

## ⚡ Plugin para Top Bar do Omarchy (Alerta Pulsante)

O VoltSteth se integra nativamente à barra superior do **Omarchy Linux** (Quickshell / Wayland) com um widget reativo:

<p align="center">
  <code>[ Estado Saudável ] ──> <b>󱐋 OK</b> (Translúcido, discreto)</code><br>
  <code>[ Loop de Terra ]  ──> <b>󱐋 🔴 GROUND LOOP! (60Hz: 108x)</b> ⮂ <b>󱐋 ⚠️ VAZAMENTO!</b> (Pisca dinamicamente)</code>
</p>

* **Alerta Visual Estroboscópico:** Quando um cabo com retorno de terra é conectado, o ícone na barra pisca alternando entre o indicador de severidade e a razão da harmônica de 60Hz.
* **Ações de Clique:**
  * **Clique Esquerdo:** Abre o painel completo do VoltSteth.
  * **Clique Direito:** Dispara notificação nativa no desktop e executa auditoria rápida em segundo plano.

---

## 🔬 O que o VoltSteth Detecta?

### 1. Loop de Terra (Mains Hum)
* Processa o sinal analógico via **algoritmo de Goertzel com janela de Hann** (eliminando vazamento espectral), caçando as frequências fundamentais e harmônicas da rede elétrica local ($60\text{ Hz}$, $120\text{ Hz}$, $180\text{ Hz}$ no Brasil/EUA ou $50\text{ Hz}$ na Europa).
* Dispara alerta quando a razão sinal/ruído da harmônica supera com significância matemática o piso do ambiente.

### 2. Triangulação Topológica e Pre-Flight Check
* Mapeia a árvore de conexões de hardware (Controladores PCI, placas ALSA e barramento USB).
* Audita os registradores do mixer ALSA e **recusa emitir laudo falso** se o canal analógico de captura estiver mutado ou zerado pelo sistema operacional.

### 3. Ripple e Instabilidade de VRM / Fonte
* Amostra os sensores de voltagem do subsistema de kernel `/sys/class/hwmon` e calcula o desvio padrão ($\sigma$) e a tensão pico-a-pico ($V_{\text{pp}}$) em janelas temporais móveis para detectar capacitores estufando ou fontes degradadas.

### 4. Acoplamento Eletromagnético (EMI & Coil Whine)
* Calcula o **coeficiente de correlação de Pearson** entre picos de ruído de alta frequência (2 kHz a 12 kHz) e pulsos de potência instantânea da CPU/GPU (via Intel RAPL / microjoules).

### 5. Erros de Barramento PCIe (AER)
* Monitora em tempo real os contadores do protocolo PCIe AER (Advanced Error Reporting) para falhas físicas na trilha do slot ou mau contato elétrico.

---

## 🔒 Segurança e Privacidade por Design (AI-Safe)

O VoltSteth foi concebido para atender a ambientes corporativos e usuários com requisitos estritos de privacidade:

* **Princípio da Não-Reconstrução (Zero Raw Audio):** O software **nunca grava, armazena ou transmite áudio bruto**. Os buffers PCM são processados em RAM para cálculo dos coeficientes de Goertzel e **descartados imediatamente**.
* **Impossibilidade Matemática de Escuta:** As únicas saídas do motor são métricas estatísticas escalares (ex: `hum_ratio`, `rms_db`, `stddev_mv`). É impossível reconstruir voz, conversas ou digitação a partir desses dados.
* **Comunicação Segura Local:** O micro-probe opera sem abrir portas de rede TCP. Todos os acessos são restritos a sockets UNIX locais com permissões de usuário `0600`.

---

## 🏗️ Arquitetura de Software (Pipe-and-Filter)

Construído em **C++20 modular**, seguindo rigorosamente os princípios **SOLID** e padrão arquitetural **Pipe-and-Filter**:

```
[ Data Sources ]               [ Filter Chain ]                     [ Sinks ]
┌──────────────────┐           ┌────────────────────────────┐       ┌─────────────────┐
│ AlsaAudioSource  │──────┐    │ GroundLoopDetector         │   ┌──>│  ConsoleHudSink │
└──────────────────┘      │    └────────────────────────────┘   │   └─────────────────┘
┌──────────────────┐      ├───>┌────────────────────────────┐───┤   ┌─────────────────┐
│ HwmonPowerSource │──────┼───>│ VoltageRippleDetector      │───┼──>│  JsonStreamSink │
└──────────────────┘      │    └────────────────────────────┘   │   └─────────────────┘
┌──────────────────┐      │    ┌────────────────────────────┐   │   ┌─────────────────┐
│  PcieAerSource   │──────┼───>│ EmiCorrelationDetector     │   └──>│ SessionRecorder │
└──────────────────┘      │    └────────────────────────────┘       └─────────────────┘
┌──────────────────┐      │
│ TopologySource   │──────┘
└──────────────────┘
```

* **Zero Hardcode:** Todos os limiares, taxas, frequências e diretórios do kernel são lidos dinamicamente de um arquivo `.ini` configurável.
* **Tolerância a Falhas:** Se o microfone ou sensor de hardware não estiver disponível, o pipeline continua executando sem abortar o processo (degradação graciosa).

---

## 🚀 Como Executar

### 1. Interface Gráfica (Recomendado):
Abra pelo menu de aplicativos do seu sistema procurando por **VoltSteth**, ou execute via terminal:
```bash
./gui/volt_steth_gui.py
```

### 2. Mapeamento de Topologia Física:
```bash
./build/volt-steth --topology
```

### 3. Auditoria Física Rápida (Laudo em 6 segundos):
```bash
./build/volt-steth --config config.default.ini --diagnose
```

### 4. Micro-Sonda em Tempo Real (<20ms para barras e scripts):
```bash
./build/volt-steth --config config.default.ini --quick-check
# Retorno JSON: {"status":"OK","ground_loop":false,"rms_db":-120.0,"hum_ratio":1.00,"anomalies":0}
```

### 5. Gravar Sessão de Telemetria Contínua (JSONL para análise por IA):
```bash
./build/volt-steth --config config.default.ini --record sessao.jsonl --duration 30
```

### 6. Executar a HUD Interativa no Terminal:
```bash
./build/volt-steth --config config.default.ini
```

---

## 📦 Compilação e Instalação

### Pré-requisitos (Arch / Omarchy Linux):
```bash
sudo pacman -S base-devel cmake alsa-lib python-gobject libadwaita jq
```

### Compilar o Core C++20:
```bash
cmake -B build
cmake --build build
```

### Executar a Suíte de Testes Automatizados:
```bash
./build/test-pipeline
```

---

## ⚡ Instalação do Widget na Barra do Omarchy

Para adicionar o ícone com alerta pulsante na top bar do Omarchy:

1. Copie o script do widget para a pasta de scripts da sua barra:
   ```bash
   cp integrations/omarchy/volt-steth-bar ~/.config/omarchy/bar/scripts/volt-steth
   chmod +x ~/.config/omarchy/bar/scripts/volt-steth
   ```
2. Adicione a entrada na seção `right` de `~/.config/omarchy/shell.json`:
   ```json
   {
     "id": "volt-steth",
     "type": "command",
     "exec": "/home/walbarellos/.config/omarchy/bar/scripts/volt-steth",
     "interval": 2,
     "onClick": "env WAYLAND_DISPLAY=wayland-1 python3 /home/walbarellos/volt-steth/gui/volt_steth_gui.py",
     "onRightClick": "notify-send 'VoltSteth' 'Iniciando auditoria física...' && /home/walbarellos/volt-steth/build/volt-steth --config /home/walbarellos/volt-steth/config.default.ini --diagnose"
   }
   ```
3. A barra recarrega automaticamente ao salvar!

---

## 📄 Licença
Distribuído sob a licença [MIT](LICENSE).
