================
The I2C Protocol
================

=============== =============================================================
S               Start condition
P               Stop condition
Rd/Wr (1 bit)   Read/Write bit. Rd equals 1, Wr equals 0.
A, NA (1 bit)   Acknowledge (ACK) and Not Acknowledge (NACK) bit
Addr  (7 bits)  I2C 7 bit address. Note that this can be expanded as usual to
                get a 10 bit I2C address.
Comm  (8 bits)  Command byte, a data byte which often selects a register on
                the device.
Data  (8 bits)  A plain data byte. Sometimes, I write DataLow, DataHigh
                for 16 bit data.
Count (8 bits)  A data byte containing the length of a block operation.

[..]            Data sent by I2C device, as opposed to data sent by the
                host adapter.
=============== =============================================================


Simple send transaction
=======================

Implemented by i2c_master_send()::

  S Addr Wr [A] Data [A] Data [A] ... [A] Data [A] P


Simple receive transaction
==========================

Implemented by i2c_master_recv()::

  S Addr Rd [A] [Data] A [Data] A ... A [Data] NA P


Combined transactions
=====================

Implemented by i2c_transfer().

They are just like the above transactions, but instead of a stop
condition P a start condition S is sent and the transaction continues.
An example of a byte read, followed by a byte write::

  S Addr Rd [A] [Data] NA S Addr Wr [A] Data [A] P

