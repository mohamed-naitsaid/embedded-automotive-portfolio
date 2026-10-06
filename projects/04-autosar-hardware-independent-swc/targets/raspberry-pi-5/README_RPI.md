AUTOSAR LED PORTABILITY - Raspberry Pi 5
=========================================

1. Objectif
-----------
Exécuter le même SWC AUTOSAR généré par Simulink :

SWC_LedControl.c

sans modifier l'Application Layer.

Chaîne :

SWC_LedControl.c
        |
        v
Rte_IWrite(...)
        |
        v
Rte_Adapter.c
        |
        v
Led_Hw_Write()
        |
        v
Led_Hw_RaspberryPi.c
        |
        v
libgpiod
        |
        v
GPIO17
        |
        v
LED


2. Copier le projet sur Raspberry Pi
------------------------------------
Copier le dossier 06_RaspberryPi5 sur le Raspberry Pi.


3. Installer les dépendances
----------------------------
sudo apt update

sudo apt install gcc make pkg-config gpiod libgpiod-dev


4. Vérifier les GPIO
--------------------
gpiodetect

gpiofind GPIO17


5. Câblage
----------
GPIO17 = pin physique 11

GPIO17 ---- 330 ohm ---- LED ---- GND


6. Compiler
-----------
cd 06_RaspberryPi5

make


7. Exécuter
-----------
./autosar_led


8. Résultat attendu
-------------------
La LED change d'état toutes les 500 ms :

OFF
ON
OFF
ON
...


9. Important
------------
Le fichier SWC_LedControl.c ne doit pas être modifié.

Seule la couche spécifique au matériel change :

Windows:
Led_Hw_Console.c

STM32:
Led_Hw_STM32.c

Raspberry Pi:
Led_Hw_RaspberryPi.c