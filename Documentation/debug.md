# Clio Debugging

## FPGA ARM

Copy paste the following code snippt on the ARM Linux.
Run `bash fpga_counter_reg.sh` to dump real time counters.

```bash
cat > fpga_counter_reg.sh <<EOF

echo ====== Lookup Registers ========
echo RequestIn
devmem 0xA0006800 32
echo SeqFifoIn
devmem 0xA0006804 32
echo SeqFifoOut
devmem 0xA0006808 32
echo CacheReqq
devmem 0xA000680C 32
echo CacheFWD
devmem 0xA0006810 32
echo CacheRES
devmem 0xA0006814 32
echo UpdateRptHit
devmem 0xA0006818 32
echo UpdateRptShootDown
devmem 0xA000681C 32
echo UpdateRptAlloc
devmem 0xA0006820 32
echo UpdateUpdate
devmem 0xA0006824 32
echo UpdateInsert
devmem 0xA0006828 32
echo ====== Memory Access Registers ========
echo ParserDataIn
devmem 0xA0006400 32
echo ParserHeaderOut
devmem 0xA0006404 32
echo ParserDataOut
devmem 0xA0006408 32
echo BuilderDataIn
devmem 0xA000640C 32
echo BuilderHeaderIn
devmem 0xA0006410 32
echo BuilderHeaderOut
devmem 0xA0006414 32
echo LookupHeader
devmem 0xA0006418 32
echo AccessHeader
devmem 0xA000641C 32
echo LookupRes
devmem 0xA0006420 32
echo writeDesc
devmem 0xA0006424 32
echo readDesc
devmem 0xA0006428 32
echo filterSignal
devmem 0xA000642C 32
echo FilteredSignal
devmem 0xA0006430 32
echo dmaWriteData
devmem 0xA0006434 32
echo dmaReadData
devmem 0xA0006438 32
echo ctrlOUt
devmem 0xA000643C 32
EOF
```
