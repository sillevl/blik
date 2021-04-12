# Blik - CAN bus protocol for broadcasting

Blik is a simple CAN bus protocol for broadcasting data on a CAN. It allows for larger messages than the 8 byte CAN2.0 limit and is inspired by the ISOTP (ISO 15765-2) standard.

## Frames

Type| Code | Description
---------|----------|---------
 Single frame | 0 | The single frame transferred contains the complete payload of up to 7 bytes (normal addressing) or 6 bytes (extended addressing).
 First frame | 1 | The first frame of a longer multi-frame message packet, used when more than 6/7 bytes of data segmented must be communicated. The first frame contains the length of the full packet, along with the initial data.
 Consecutive frame | 2 | A frame containing subsequent data for a multi-frame packet.

 The maximum data size is 118 bytes (844 bits) and limited by the combination of 1 first frame (6 bytes) and 16 consecutive frames (7 bytes each) => 6 + (16 * 7) = 118.

## Blik headers

Bit offset | 7..4 (byte 0) | 3..0 (byte 0) | 15..8 (byte 1) | 23..16 (byte 2) | ...
---------|----------|---------|---------|----------|---------
 Single | 0 | size(0..7) | Data A | Data B | Data C
 First | 1 | size(8..118) | size(8..118) | Data A | Data B
 Consecutive | 2 | index(0..15) | Data A | Data B | Data C
