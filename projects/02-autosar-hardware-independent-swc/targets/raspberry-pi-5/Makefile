CC = gcc

CFLAGS = -Wall -Wextra -std=c11 \
         -Iapp \
         -Irte \
         -Icompat \
         -Iplatform \
         $(shell pkg-config --cflags libgpiod)

LDLIBS = $(shell pkg-config --libs libgpiod)

SRC = app/SWC_LedControl.c \
      rte/Rte_Adapter.c \
      platform/Led_Hw_RaspberryPi.c \
      main_rpi.c

TARGET = autosar_led

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET)