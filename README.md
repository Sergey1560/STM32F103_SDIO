# STM32F429, SDIO 4-bit

Пример работы с SD картой через SDIO 4-bit интерфейс и бибилиотеку FATFs.
Чтение и запись происходят через DMA.
Режим HighSpeed не работает, после влкючения CLK_BYPASS карта не инициализируется.
Работает на тактовой 72Mhz (36Mhz на карту) и 48Mhz (24 Mhz на карту). Поскольку
CLK_BYPASS не включен, реальная частота на карту в 2 раза меньше.

Оптимизация -O2:

Write file: 4177920 bytes in 3098 ms, speed 1348 Kbyte/sec
Read file:  4177920 bytes in 3216 ms, speed 1299 Kbyte/sec
RAW Read:   4177920 bytes in 325 ms, speed 12855 Kbyte/sec
