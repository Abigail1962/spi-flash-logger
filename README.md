# SPI flash fault logger: host simulation

SPI transfers run against a RAM-backed NOR model. No physical W25Q chip has been
tested. Data persists across Logger_Init in one process, not process restarts.

~~~sh
make all test
./spi_logger_sim
~~~

Writes split at 256-byte page boundaries. The model wraps page programs within
a page, programs only 1-to-0 bits, and starts erased (0xFF). Busy polling is
bounded by host iterations and returns an error.

The logger stores a 14-byte explicitly serialized payload in 16-byte slots.
This NEW layout is incompatible with the old 14-byte stride: existing on-device
logs need migration/versioning before deployment. Slots cannot cross pages or
sectors. Boot scanning finds the newest valid sequence, skips damaged slots, and
reclaims sectors on wrap. Timestamp is a sequence, not elapsed seconds.
UINT32_MAX is reserved; sequence exhaustion returns an error. Printing uses
physical slot order, not chronological order after wrap.

Tests cover CRC, page splits, NOR bit behavior, sector crossing, an explicitly
incomplete programmed record, busy timeout, and repeated wrap. CRC rejection is
not an atomic-write or arbitrary power-failure guarantee. Hardware timing,
endurance, brownout behavior and a versioned commit protocol remain unvalidated.
