# Angebot für das Projekt "Statische Binärübersetzung von RISC-V auf x86-64"

## Beschreibung

Das Hauptziel des Projekts ist es, ein Programm zu entwickeln, das RISC-V Binaries in x86_64 Assembler übersetzen kann. Die zu entwickelnden Komponenten sind eine selbst definierte IR, ein RISC-V Lifter, ein Optimizer für die IR und ein Compiler der die optimierte IR in x86_64 GNU Assembler compilieren kann. Anfangs konzentrieren wir uns auf die Übersetzung von statische gelinkten Binaries um ABI Konflikte weitgehend zu vermeiden. Außerdem planen wir erstmal nur die unten genannten ISA Extensions zu unterstützen. 

## Hauptziele:

* Decodierung von statisch gelinkten little-endian RISC-V Binaries
* Unterstützung der (ratified) Base ISA Extensions zusätzlich M, Zicsr, Zifencei, C
* Definition einer eigenen, SSA basierten IR (siehe [Issue #2](https://gitlab.lrz.de/lrr-tum/students/eragp-sbt-2021/-/issues/2))
* Lifting der RISC-V Binaries in diese eigene IR
* Erkennung von BasicBlöcken (d.h. Blöcke ohne Kontrollflussänderung), Funktionen und RISC-V typischen Strukturen
* Übersetzung der IR zu x86_64 GNU Assembler, der nach assemblieren zusammen mit weiteren Laufzeitkomponennten (Hilfsfunktionen und Interpreter) zu einer nativ auf x86_64 ausführbaren Binary gelinkt werden kann.

## Erweiterte Ziele:

* Begrenzte Unterstützung von RISC-V Code mit unbekanntem Sprungziel oder selbst modifizierendem Code der zur Laufzeit JIT-Interpretiert wird
* Dynamisch gelinkte Libraries durch JIT-Interpretierung/ebenfalls statische Übersetzung unterstützen (oder Benutzung der x86_64 Libraries, ABI Konflikte)
* Optimierung der IR und x86_64 Registernutzung durch Wissen über die genannten Strukturen und RISC-V ABI Konventionen
* Optimierung der RISC-V typischen Strukturen wie z.B. Jumptables
* Multithreading Support
* Floating Point Operations (R64F, R64D)
* Support für weitere ISA Extensions (z.B. R64Q, R64A)