<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-HMI banner" width="100%">
</p>

# 🖥️ EH3D-NODE-BUNGARD-HMI

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | 🇵🇹 <b>Português</b></p>

### Um ecrã tátil de 10,1 polegadas com um ESP32-P4 que controla sozinho uma máquina Bungard CCD: os ecrãs e o fluxo do programa de secretária, em oito idiomas, avisos e ordens por voz, e uma paragem por hardware que não passa pelo ecrã.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que corre hoje:** as regras do ecrã estão escritas em C++17 e testadas num computador: o ficheiro de definições com os valores seguros por omissão, a comporta que cada movimento e cada ordem do fuso tem de passar (uma confirmação no ecrã, o trinco da paragem por hardware, a ligação armada), as ordens e avisos por voz em oito idiomas e os textos dos ecrãs. O firmware ESP-IDF com os ecrãs LVGL está escrito para o módulo Guition JC8012P4A1C mas **não foi compilado, gravado nem executado num módulo**, ainda não há ligação ao servidor nem à máquina e nada foi ligado a uma máquina.

---

> **Estado de segurança (safety) e de segurança informática:** isto não é um sistema de segurança (safety). A paragem que protege as pessoas e a máquina é um botão por hardware que corta o acionamento por si, ligado à parte deste ecrã; o ecrã só o lê, mostra-o e recusa-se a mover o que quer que seja enquanto estiver premido ou por reconhecer. Um movimento ou uma ordem do fuso pede uma confirmação no ecrã, uma ordem por voz pede-a sempre, e as ordens por voz estão desligadas até o operador as ligar. Não uses este ecrã para operar uma máquina até toda a cadeia ter sido testada numa real. O projeto é independente: não é feito, aprovado nem apoiado pela Bungard Elektronik ou pela Guition, e os nomes de produtos aparecem apenas para dizer com que máquinas e módulos se destina a funcionar.

## 📋 Num relance

| | |
|---|---|
| Versão | `0.0.1` |
| Maturidade | Estrutura inicial |
| Testes | 1 programa de teste no computador (598 verificações contadas) |
| Tecnologias | C++17, ESP-IDF 5.5, LVGL 9, ESP32-P4 |
| Em hardware real | Não: o firmware não foi compilado nem gravado e nada foi ligado a uma máquina |

**Ir para:** [Visão geral](#-visão-geral) · [Arquitetura](#-arquitetura) · [Início rápido](#-início-rápido) · [Testes](#-testes) · [Estrutura](#-estrutura-do-repositório) · [Relacionados](#-projetos-relacionados)

## 🚦 Estado do projeto

Cada linha é uma parte deste repositório e a luz diz até onde chega hoje. É escrita com o que o repositório consegue demonstrar (testes, compilações, execuções), não com o que está previsto.

🟢 Feito e verificado · 🟡 Funciona, com limites conhecidos · 🟠 Feito a meio ou ainda por verificar · 🔴 Por fazer ou a não funcionar

| Área | Estado | O que significa aqui |
|---|---|---|
| Software | 🟠 | O projeto ESP-IDF (ESP32-P4, LVGL 9, o ficheiro de definições na sua própria partição) está escrito; não foi compilado nem gravado. |
| Testes | 🟢 | Passa 1 programa de teste no computador (`ctest`): 598 verificações do ficheiro de definições, da comporta, das ordens e avisos por voz e dos textos dos ecrãs em oito idiomas. |
| Regras de segurança | 🟡 | Todo o movimento e toda a ordem do fuso passam pela comporta: confirmação no ecrã, trinco da paragem por hardware que só um reconhecimento no ecrã levanta com o botão solto, nenhum movimento com a ligação por armar. Coberto por testes; nunca experimentado com um botão de paragem real. |
| Voz | 🟡 | As ordens e avisos por voz são reconhecidos e formulados em oito idiomas e testados como texto; não se usou nenhum microfone nem altifalante. |
| Ecrãs | 🟠 | O ecrã da máquina (movimento manual, origem, fuso, início, paragem, confirmação, aviso de paragem por hardware) está escrito; os separadores visor, trabalho, máquinas e estações e definições ainda são páginas vazias. |
| Hardware real | 🔴 | Nenhum módulo executou este firmware, a ligação ao servidor ainda não existe e nada foi ligado a uma máquina. |
| Documentação | 🟢 | README em oito idiomas, a referência das definições e as notas de hardware. |

## 🎯 Visão geral

* A **comporta**: todo o movimento e toda a ordem do fuso, venha de um toque ou da voz, passa por um único sítio que decide executá-lo, pedir uma confirmação ou recusá-lo com um motivo; uma paragem passa sempre.
* O **ficheiro de definições** `hmi.cfg`: todo o valor afinado (idioma, brilho, o servidor, os interruptores de segurança, a voz) é uma linha de um ficheiro na sua própria partição, nunca um número no código; um valor fora do intervalo é assinalado e fica o valor seguro por omissão.
* A **voz**: avisos falados (trabalho terminado, mudança de ferramenta, erro, ligação perdida, paragem por hardware) e uma lista curta e fixa de ordens por voz em oito idiomas; as ordens que só param entendem-se dentro de uma frase, as que movem ou rodam têm de ser ditas inteiras e pedem sempre uma confirmação.
* Os **ecrãs**: a máquina (movimento manual, origem, fuso, início, paragem), o trabalho, o visor, as máquinas e estações do servidor e as definições, pela ordem e com os nomes do programa de secretária; o botão de paragem está sempre no topo.
* O **módulo**: o Guition JC8012P4A1C (ESP32-P4 com 32 MB de RAM externa, ecrã tátil MIPI-DSI de 10,1 polegadas e 800x1280, um ESP32-C6 para o Wi-Fi, o codec ES8311 com amplificador de altifalante e a entrada de microfone ES7210). O painel da unidade é o novo.

## 🧩 Papel no EH3D

EH3D-NODE-BUNGARD-HMI é o ecrã próprio do operador de uma máquina Bungard CCD: ocupa o lugar do PC junto à máquina. Destina-se a comandar a máquina através do EH3D-SERVER ou do EH3D-NODE-BUNGARD (o nó da porta série da máquina), tal como o EH3D-DRILLPRO e o EH3D-ROUTEPRO. Essa ligação ainda não está escrita.

## 🏛️ Arquitetura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitetura de EH3D-NODE-BUNGARD-HMI: quem o usa, o que contém e de que depende" width="100%">
</p>

Como as partes deste repositório se encaixam: quem fala com ele, o que contém e de que depende. O desenho sai de `docs/architecture.json` e é verificado contra o código.

## ✨ Funcionalidades

* **Uma paragem que não se esquece:** o botão de paragem está sempre no topo e funciona sempre; a paragem por hardware fica trancada e só um reconhecimento no ecrã, com o botão solto, a levanta.
* **Confirmação antes do movimento:** um movimento ou uma ordem do fuso mostra o que vai fazer e espera um toque, com tempo limite; um novo pedido ou uma ligação perdida descarta-o.
* **Oito idiomas, também por voz:** inglês, espanhol, italiano, alemão, francês, português de Portugal, chinês e japonês nos ecrãs, nos avisos falados e nas ordens por voz.
* **Seguro por omissão:** cada interruptor de segurança tem a sua definição e começa na posição prudente: ordens por voz desligadas, confirmação por voz desligada, confirmação ligada, paragem por hardware exigida.

## 🔒 Segurança e proteção

| Ameaça | Defesa |
|---|---|
| Um toque ou uma ordem por voz que move a máquina por acidente | um movimento pede uma confirmação no ecrã com tempo limite; uma ordem por voz pede-a sempre; um sim falado só confirma com a definição `voice_confirm` ligada (desligada por omissão) |
| O ecrã começa a mover com a paragem por hardware premida | a paragem por hardware é um trinco: premida, ou solta mas por reconhecer no ecrã, recusa-se todo o movimento e toda a ordem do fuso e descarta-se o que esperava; uma paragem premida enquanto uma confirmação espera cancela-a |
| Uma máquina que não está pronta | nada se move com a ligação por armar, com a entrada de paragem por hardware por ligar (`hw_stop_required`, ligada por omissão) ou com o limite do fuso a 0 |
| Uma palavra falada tomada por uma ordem | as ordens vêm de uma lista curta e fixa por idioma, nunca de texto livre; uma palavra que só contém uma ordem (`stopwatch`) não é uma ordem; as ordens por voz estão desligadas até o operador as ligar |
| Um erro de escrita nas definições | um valor fora do intervalo ou uma chave desconhecida é assinalado e fica o valor seguro por omissão; nenhum interruptor se abre por engano |
| Um segredo no firmware ou no repositório | nada de secreto é compilado; o token é uma definição na sua partição e nunca é mostrado nem registado |

**O que não está defendido:** o ecrã não é um dispositivo de segurança e não pode cortar o acionamento da máquina; isso tem de o fazer a paragem por hardware por si. Ainda não há ligação ao servidor, por isso hoje o ecrã não pode mover nenhuma máquina. O reconhecimento de voz é testado como texto; o caminho real do microfone (cancelamento de eco, ruído, sotaques) não foi experimentado. Quem tiver o módulo nas mãos pode alterar o ficheiro de definições.

## 🔌 Interfaces

| Interface | O que é | Onde |
|---|---|---|
| Ficheiro de definições | `hmi.cfg` na partição `config`: linhas `chave = valor`; ver `hmi.cfg.example` e a tabela acima | `core/hmi_config.hpp` |
| Entrada de paragem por hardware | um GPIO (`HMI_HW_STOP_GPIO` no menuconfig, ativo em baixo por omissão, com pull-up para que um fio aberto conte como premido) | `main/board_p4.cpp` |
| Ecrã tátil | MIPI-DSI de 800x1280 através do LVGL 9 | `main/ui.cpp` |
| Som | codec ES8311 com amplificador de altifalante e a entrada de microfone ES7210 | `main/board_p4.cpp` |

## ⚙️ Configuração

Tudo o que é afinado é uma linha de `hmi.cfg` (copia `hmi.cfg.example` para a partição `config`); um valor fora do intervalo é assinalado no registo e fica o valor por omissão. Os valores abaixo são os de omissão; todos os de segurança são os prudentes.

| Definição | Omissão | Intervalo | O que faz |
|---|---|---|---|
| `language` | `en` | `en es it de fr pt zh ja` | o idioma dos ecrãs e da voz |
| `confirm_moves` | on | on / off | pede confirmação para mover: desligar só na bancada |
| `confirm_timeout_ms` | 8000 | 1000 - 60000 | quanto tempo espera uma confirmação |
| `jog_max_mm` | 20 | 1 - 200 | o movimento manual mais longo que o ecrã envia |
| `spindle_max_rpm` | 24000 | 0 - 65535 | a velocidade máxima do fuso que pede (0: nunca arranca o fuso) |
| `hw_stop_required` | on | on / off | só comanda a máquina com a entrada de paragem por hardware ligada e solta |
| `voice_orders` | off | on / off | aceita ordens por voz |
| `voice_confirm` | off | on / off | aceita um sim falado como confirmação de um movimento |
| `token` | none | 8 - 64 characters | o token de acesso ao servidor; só de escrita, nunca mostrado nem registado |

## 🚀 Início rápido

Precisas de um compilador C++17 e do CMake para os testes no computador, e do ESP-IDF 5.5.5 para o firmware (o alvo ESP32-P4 do módulo).

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
python -I tools/ci_validate.py
```

O firmware ainda não foi compilado: quando for, `idf.py set-target esp32p4 && idf.py build` gera a imagem e uma partição chamada `config` guarda `hmi.cfg`.

## 🧪 Testes

Os testes no computador (`build.bat` ou `./build.sh`, e depois `ctest`) são um programa, `test_hmi`, e passa tudo: 598 verificações do ficheiro de definições (valores por omissão, intervalos, erros, fins de linha), da comporta (a paragem passa sempre, trinco da paragem por hardware, confirmação, tempo limite e volta do relógio, regras de voz, interruptores), das ordens e avisos por voz em oito idiomas e dos textos dos ecrãs.

## 🏗️ Compilação e publicação

A versão vive em `eh3d.project.json` e cada versão tem a sua entrada em [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilam e executam os testes no computador e `test.sh` / `test.bat` executam as verificações do repositório. O firmware compila-se fora dos scripts deste repositório, numa janela visível, uma compilação de cada vez.

## 🩺 Resolução de problemas

| Sintoma | O que fazer |
|---|---|
| Nada se move e o ecrã diz que a paragem por hardware está premida ou por reconhecer | Solta o botão e prime o botão de reconhecer do ecrã: uma paragem que foi solta não se levanta sozinha. |
| O ecrã diz que a máquina não está armada | Nesta versão ainda não há ligação à máquina; a comporta recusa todo o movimento até uma ligação a armar. |
| O ecrã diz que a entrada de paragem por hardware não está ligada | Liga o botão de paragem ao pino definido no menuconfig (`HMI_HW_STOP_GPIO`), ou desliga `hw_stop_required` apenas na bancada. |
| Uma ordem por voz não faz nada | As ordens por voz estão desligadas por omissão: põe `voice_orders = on`. As que movem ou rodam pedem um toque no ecrã, salvo se `voice_confirm` estiver ligado. |

## 📂 Estrutura do repositório

<p align="center">
  <img src="images/repository-structure.svg" alt="Estrutura do repositório EH3D-NODE-BUNGARD-HMI" width="100%">
</p>

<details>
<summary>Versão em texto da árvore</summary>

```text
EH3D-NODE-BUNGARD-HMI/
├── core/
│   ├── hmi_config.hpp lang.hpp                  o ficheiro de definições e os seus valores seguros
│   ├── gate.hpp                                 a comporta: confirmação, trinco da paragem
│   ├── voice.hpp                                ordens e avisos por voz em oito idiomas
│   └── ui_text.hpp                              os textos dos ecrãs em oito idiomas
├── main/
│   ├── app_main.cpp                             arranque, definições, o ciclo da comporta
│   ├── ui.cpp ui.hpp                            os ecrãs LVGL e o botão de paragem
│   ├── board_p4.cpp board.hpp                   o módulo: ecrã, som, entrada de paragem
│   └── idf_component.yml Kconfig.projbuild      lista de componentes e pinos do menuconfig
├── tests/                                       testes no computador das definições, da comporta, da voz e dos textos de ecrã
├── docs/                                        as notas de hardware e as fontes dos dois desenhos
├── tools/                                       verificação de CI, de README e ferramentas dos desenhos
├── CMakeLists.txt                               o projeto ESP-IDF
├── partitions.csv                               a divisão da flash com duas ranhuras
├── hmi.cfg.example                              cada definição com o seu valor por omissão e intervalo
├── sdkconfig.defaults                           definições do ESP-IDF para o módulo ESP32-P4
├── images/                                      banner e as imagens de Arquitetura e Estrutura do repositório
├── .github/                                     fluxos de CI e de publicação, modelos de issues e pull requests
├── build.sh build.bat                           compila e executa os testes no computador num painel em direto
├── eh3d.project.json                            manifesto: versão, papel, maturidade e comando de compilação
├── CHANGELOG.md                                 o que mudou em cada versão
├── SECURITY.md SUPPORT.md                       como comunicar uma vulnerabilidade e onde perguntar
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   como contribuir, o código de conduta e a licença
└── bump_manifest_version.py                     utilitário de versão do manifesto
```

</details>

## 🗺️ Roteiro e limites conhecidos

**Limites conhecidos hoje**

* Nada correu num módulo: o firmware não foi compilado, gravado nem experimentado, e o pacote de suporte de placa que pede pode precisar de alterações para este módulo.
* Ainda não há ligação ao EH3D-SERVER nem ao nó da máquina, por isso os ecrãs não podem mover uma máquina; os separadores visor, trabalho, máquinas e estações e definições são páginas vazias.
* O pino da paragem por hardware e o controlador tátil ainda não estão fixados: têm de ser verificados com o esquema da unidade.
* O reconhecedor de voz e o altifalante ainda não estão no firmware; as ordens e avisos são testados como texto.
* A lógica dos programas de secretária (formatos, controlador, frota) não é reutilizada: os ecrãs seguem o programa de secretária, mas as regras são reescritas em C++.

**A seguir**

* Compilar o firmware e arrancar o módulo: ecrã, tátil e som.
* Escrever a ligação ao EH3D-SERVER e ao nó, e preencher os outros separadores dos ecrãs.
* Juntar o reconhecedor de voz e o altifalante e experimentá-los com cancelamento de eco.
* Ligar um botão de paragem real e experimentar o trinco numa máquina.

## 🌍 Idiomas

O README e os textos existem em inglês, espanhol, francês, italiano, alemão, chinês, japonês e português europeu.

## 🔗 Projetos relacionados

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): o programa de secretária que os ecrãs seguem
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): o programa de fresagem; partilha o ficheiro de máquina e a biblioteca do protocolo
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): o centro da família: executa os trabalhos de cada cliente
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): o nó da porta série da unidade de controlo da máquina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): as partes comuns da família (formatos, controlador, frota, temas)

## 📚 Documentação

* As definições: `hmi.cfg.example` lista cada chave com o seu valor por omissão e o intervalo.
* O hardware: os pinos do módulo e as notas que acompanham a demo do fabricante, em `docs/HARDWARE.md`.
* [Registo de alterações](CHANGELOG.md)

## 🤝 Contribuir e comunidade

* Antes de um pull request executa os testes no computador (`./build.sh` ou `build.bat`) e `python -I tools/ci_validate.py`: tudo tem de passar. Vê [CONTRIBUTING.md](CONTRIBUTING.md) e o [código de conduta](CODE_OF_CONDUCT.md).
* O README existe em oito idiomas: uma alteração do texto faz-se nos oito ficheiros, e os dois desenhos saem de `docs/architecture.json` e `docs/structure.json` com `tools/make_architecture.py` e `tools/make_structure.py`.
* Nada que permita ao ecrã mover uma máquina muda sem um teste da comporta que o cubra e uma nota do que foi observado em hardware.
* Comunica uma vulnerabilidade em privado: [SECURITY.md](SECURITY.md). Perguntas, ideias e relatórios: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENÇA

Software: GPL-3.0-or-later. Ver [LICENSE](LICENSE). Os projetos de hardware da suite usam a CERN OHL-S.
