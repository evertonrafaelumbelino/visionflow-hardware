# 👓 VisionFlow — Óculos Inteligentes de Auxílio à Mobilidade para Deficientes Visuais

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org/)
[![C++](https://img.shields.io/badge/Language-C++-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> *"Transformando percepção visual em feedback tátil e sonoro para promover independência, segurança e inclusão."*

## 🚀 Sobre o Projeto (O Problema e a Solução)

A deficiência visual severa impacta diretamente a autonomia de milhões de pessoas, tornando a locomoção urbana e a identificação de obstáculos em ambientes desconhecidos (especialmente obstáculos na altura do tronco, cabeça e desníveis no chão) tarefas diárias desafiadoras e estressantes. 

O **VisionFlow** surge como uma solução de tecnologia assistiva de baixo custo e alta eficiência. Trata-se de um par de óculos inteligentes que atua como um "segundo par de olhos" para o usuário. O sistema mapeia o ambiente frontal e o relevo do solo em tempo real através de sensores avançados (Laser ToF e Ultrassônico), processando as informações e convertendo-as em alertas sonoros claros via condução óssea/amplificador digital e feedback tátil por buzzer, permitindo que o usuário desvie de perigos iminentes com total naturalidade.

## ⚡ Funcionalidades Principais (Features)

* **Detecção de Obstáculos Frontais:** Varredura contínua de curto e longo alcance utilizando sensor Laser ToF (VL53L0X).
* **Monitoramento de Relevo e Chão:** Identificação de desníveis ou obstáculos inferiores através de sensor ultrassônico (HC-SR04) de alta precisão.
* **Sistema Sonoro Integrado:** Emissão de alertas nítidos e tons direcionados por áudio digital I2S (Módulo MAX98357A) e transdutores de condução óssea/buzzer ativo.
* **Conectividade Sem Fio:** Pilha Bluetooth Low Energy (BLE) integrada para comunicação com aplicativos e dispositivos móveis.
* **Arquitetura Modular:** Divisão inteligente de processamento entre o ESP32 Principal (sensores e áudio) e o ESP32-CAM (visão computacional).

## 🛠️ Arquitetura do Sistema e Componentes (Hardware e Software)

### Hardware
O protótipo físico foi construído utilizando componentes compactos e de baixo consumo energético acoplados à estrutura dos óculos:

| Componente | Função Principal | Pinos de Conexão (Resumo) |
| :--- | :--- | :--- |
| **ESP32-32U** | Microcontrolador Principal (Gerenciamento geral) | Central de controle |
| **ESP32-CAM** | Captura de imagens e processamento de visão | Comunicação Serial |
| **Sensor Laser ToF (VL53L0X)** | Medição precisa de obstáculos frontais | Barramento I2C (`SDA`, `SCL`) |
| **Sensor Ultrassônico (HC-SR04)** | Detecção de solo e desníveis | Pinos Digitais (`Trig`, `Echo`) |
| **Módulo Amplificador I2S (MAX98357A)**| Conversão e amplificação de áudio digital | Pinos I2S (`BCLK`, `LRC`, `DOUT`) |
| **Buzzer Ativo / Transdutores** | Emissão de alertas sonoros e táteis | Pino Digital de acionamento |

### Software e Organização do Repositório
O projeto utiliza o **PlatformIO** para gerenciamento de dependências e compilação, estruturado em um formato modular (Monorepo) dividindo as responsabilidades de hardware:

```text
visionflow-hardware/
│
├── esp32_cam/             # Módulo de Câmera e Visão Computacional
│   ├── include/           # Cabeçalhos de configuração (config_cam.h)
│   └── src/               # Código principal (main.cpp)
│
└── esp32_main/            # Módulo Principal (Sensores, Áudio I2S, BLE)
    ├── include/           # Cabeçalhos de pinagem e constantes (config.h)
    └── src/               # Código principal e rotinas de diagnóstico (main.cpp)
```
## ⚙️ Configuração e Instalação (Como Replicar)
Para compilar e enviar o firmware para os microcontroladores, certifique-se de possuir os seguintes pré-requisitos instalados:

- Visual Studio Code
- Extensão PlatformIO IDE instalada no VS Code.

### Passos para clonar e compilar:
1. Clone este repositório em sua máquina local:

```bash
git clone [https://github.com/evertonrafaelumbelino/visionflow-hardware.git](https://github.com/evertonrafaelumbelino/visionflow-hardware.git)
```
2. Abra a pasta visionflow-hardware no Visual Studio Code.

3. Conecte o ESP32 Principal via cabo USB ao computador (Dica: certifique-se de isolar a alimentação externa de 5V durante o processo de gravação via USB).

4. Navegue até a pasta correspondente ao projeto (esp32_main) e utilize o PlatformIO para compilar e fazer o upload do código:

O PlatformIO gerencia automaticamente as bibliotecas (Adafruit_VL53L0X, driver I2S, etc.)

## 🚀 Como Executar (Modo de Uso)
- Alimentação: Conecte a bateria ou fonte de 5V regulada aos pinos de alimentação do circuito dos óculos.

- Inicialização do Sistema: Ao ligar o ESP32 Principal, o sistema executará uma rotina diagnóstica:
    - Emitirá bips curtos pelos transdutores de áudio para validar o sistema I2S.
    - Fará a varredura e inicialização do sensor I2C (VL53L0X).
    - Iniciará o anúncio Bluetooth (VisionFlow_TestMode).

- Monitoramento Serial: Para acompanhar as leituras em tempo real dos sensores (Laser e Ultrassônico), abra o Monitor Serial na taxa de transmissão:

```Plaintext
Baud Rate: 115200
```

- Operação Prática: Aproxime a mão ou obstáculos da lente laser frontal e do sensor ultrassônico inferior para verificar o feedback sonoro proporcional gerado pelo sistema.

## 📊 Demonstração e Resultados (Dados da Feira)
- Validação de Bancada: Os testes práticos comprovaram excelente estabilidade no sistema de áudio digital I2S (sem interferências ou ruídos) e resposta linear e precisa do sensor ultrassônico na detecção de solo.

- Fotos do Protótipo / Estação de Teste:
(Insira aqui fotos dos óculos montados, do circuito impresso/protoboard ou da equipe apresentando na feira)

## 📄 Licença

Este projeto é um trabalho acadêmico desenvolvido no âmbito da Robótica do **Colégio Estadual David Carneiro**, distribuído sob a licença [MIT](LICENSE).

## 👥 Equipe e Contexto Acadêmico

Este projeto está em desenvolvimento para apresentação na **GeniusCon 2026** por uma equipe de robótica do Colégio Estadual David Carneiro (Guapirama - PR).

**Estudantes:**
- [Everton Rafael Umbelino dos Santos](https://github.com/evertonrafaelumbelino)
- Karolaine Ribeiro de Brito
- Gabriel Rodrigues Silva de Souza
- Ana Clara do Prado Eggeia

**Professora orientadora:**
- [Ana Crispim de Sousa Castro](https://github.com/AnaCrispim)